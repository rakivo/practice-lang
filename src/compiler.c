#include "lexer.h"
#include "lib.h"
#include "ast.h"
#include "common.h"
#include "compiler.h"

#include "stb_ds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#undef report_error
#define report_error(fmt, ...) do { \
	report_error_noexit(__func__, __FILE__, __LINE__, fmt, __VA_ARGS__); \
	compiler_emergency_clean(); \
	exit(EXIT_FAILURE); \
} while (0)

#define wln(...) fprintf(stream, __VA_ARGS__ "\n")
#define wprintln(fmt, ...) fprintf(stream, fmt "\n", __VA_ARGS__)

#define TAB "\t"
#define wtln(...) wln(TAB __VA_ARGS__)
#define wtprintln(fmt, ...) wprintln(TAB fmt, __VA_ARGS__)

static FILE *stream = NULL;

static size_t label_counter = 0;
static size_t while_label_counter = 0;
static size_t string_literal_counter = 0;

static const char **strs = NULL;

static bool used_strlen = false;
static bool used_dmp_i64 = false;

static const char *X86_64_LINUX_CONVENTION_REGISTERS[6] = {
	"rdi", "rsi", "rdx", "r10", "r8", "r9"
};

typedef struct {
	bool is_used;
	ast_id_t ast_id;
	ast_kind_t ast_kind;
} value_t;

struct {
	const char *key;
	value_t value;
} *values_map = NULL;

static void
compile_ast(Compiler *ctx, const ast_t *ast);

static void
compiler_deinit(void);

static void
compiler_emergency_clean(void);

INLINE void
scratch_buffer_genstr(void)
{
	scratch_buffer_clear();
	scratch_buffer_printf("__str_%zu__", string_literal_counter);
}

INLINE void
scratch_buffer_genstrlen(void)
{
	scratch_buffer_clear();
	scratch_buffer_printf("__str_%zu_len__", string_literal_counter);
}

// Compile an ast till the `ast.next` is greater than or equal to `0`.
// Every non-empty block should be with an `ast.next` = -1 at the end.
INLINE ast_id_t
compile_block(Compiler *ctx, ast_t ast)
{
	while (ast.ast_id < asts_len) {
		compile_ast(ctx, &ast);
		if (ast.next < 0) break;
		else ast = astid(ast.next);
	}
	return ast.ast_id;
}

INLINE
void stack_add_type(Compiler *ctx, value_kind_t type)
{
	if (ctx->proc_ctx.stmt != NULL) {
		if (unlikely(ctx->proc_ctx.stack_size + 1 >= MAX_STACK_TYPES_CAP)) {
			eprintf("EMERGENCY CRASH, STACK IS TOO BIG\n");
			exit(1);
		}
		ctx->proc_ctx.stack_types[ctx->proc_ctx.stack_size++] = type;
	} else if (ctx->func_ctx.stmt != NULL) {
		if (unlikely(ctx->func_ctx.stack_size + 1 >= MAX_STACK_TYPES_CAP)) {
			eprintf("EMERGENCY CRASH, STACK IS TOO BIG\n");
			exit(1);
		}
		ctx->func_ctx.stack_types[ctx->func_ctx.stack_size++] = type;
	}
}

INLINE
void stack_pop(Compiler *ctx)
{
	if (ctx->proc_ctx.stmt != NULL)
		ctx->proc_ctx.stack_size--;
	else if (ctx->func_ctx.stmt != NULL)
		ctx->func_ctx.stack_size--;
}

INLINE const value_kind_t *
stack_at(const Compiler *ctx, size_t idx)
{
	if (ctx->proc_ctx.stmt != NULL) return &ctx->proc_ctx.stack_types[idx];
	else if (ctx->func_ctx.stmt != NULL) return &ctx->func_ctx.stack_types[idx];
	else return NULL;
}

UNUSED INLINE value_kind_t *
stack_at_mut(Compiler *ctx, size_t idx)
{
	if (ctx->proc_ctx.stmt != NULL) return &ctx->proc_ctx.stack_types[idx];
	else if (ctx->func_ctx.stmt != NULL) return &ctx->func_ctx.stack_types[idx];
	else return NULL;
}

INLINE size_t
get_stack_size(const Compiler *ctx)
{
	if (ctx->proc_ctx.stmt != NULL) return ctx->proc_ctx.stack_size;
	else if (ctx->func_ctx.stmt != NULL) return ctx->func_ctx.stack_size;
	else return 0;
}

void
stack_dump(const Compiler *ctx)
{
	const size_t size = get_stack_size(ctx);
	for (int i = size, j = 0; i > 0; --i, ++j) {
		printf("  %d: %s\n", j, value_kind_to_str_pretty(*stack_at(ctx, i - 1)));
	}
}

INLINE const value_kind_t *
get_type_from_end(const Compiler *ctx, size_t idx)
{
	const i32 idx_ = get_stack_size(ctx) - idx - 1;
	if (idx_ < 0) return NULL;
	return stack_at(ctx, idx_);
}

const value_kind_t *first_type = NULL;
const value_kind_t *second_type = NULL;
const value_kind_t *third_type = NULL;

// Check the stack size and types of values on the stack before performing a binop.
void
check_for_two_integers_on_the_stack(const Compiler *ctx, const char *op, const ast_t *ast)
{
	first_type = get_type_from_end(ctx, 1);
	second_type = get_type_from_end(ctx, 0);

	if (second_type == NULL || first_type == NULL) {
		report_error("%s error: `%s` stack underflow, bruv", loc_to_str(&locid(ast->loc_id)), op);
	} else if (*first_type  != VALUE_KIND_INTEGER
				 ||  *second_type != VALUE_KIND_INTEGER)
	{
		report_error("%s error: expected two integers on the stack, but got: `%s` and `%s`",
								 loc_to_str(&locid(ast->loc_id)),
								 value_kind_to_str_pretty(*second_type),
								 value_kind_to_str_pretty(*first_type));
	}
}

// Check the stack size and types of values on the stack before performing a binop.
void
check_for_two_types_on_the_stack(const Compiler *ctx, const char *op, const ast_t *ast,
																 value_kind_t first_type_, value_kind_t first_type_or_,
																 value_kind_t second_type_, value_kind_t second_type_or_)
{
	first_type = get_type_from_end(ctx, 0);
	second_type = get_type_from_end(ctx, 1);

	if (second_type == NULL || first_type == NULL) {
		report_error("%s error: `%s` stack underflow, bruv", loc_to_str(&locid(ast->loc_id)), op);
	} else if ((*first_type  != first_type_  && *first_type  != first_type_or_)
				 ||  (*second_type != second_type_ && *second_type != second_type_or_))
	{
		report_error("%s error: expected to have `%s`s or `%s`s on the stack, but got: `%s` and `%s`",
								 loc_to_str(&locid(ast->loc_id)),
								 value_kind_to_str_pretty(first_type_),
								 value_kind_to_str_pretty(first_type_or_),
								 value_kind_to_str_pretty(*second_type),
								 value_kind_to_str_pretty(*first_type));
	}
}

INLINE void
check_for_integer_on_the_stack(const Compiler *ctx, const char *op, const char *msg, const ast_t *ast)
{
	const value_kind_t *type = get_type_from_end(ctx, 0);

	if (type == NULL) {
		report_error("%s error: `%s` with an empty stack", loc_to_str(&locid(ast->loc_id)), op);
	} else if (*type != VALUE_KIND_INTEGER) {
		report_error("%s error: %s, but got: %s",
								 loc_to_str(&locid(ast->loc_id)),
								 msg,
								 value_kind_to_str_pretty(*type));
	}
}

INLINE value_kind_t
check_stack_for_last(const Compiler *ctx, const char *op, const ast_t *ast)
{
	const value_kind_t *type = get_type_from_end(ctx, 0);
	if (type == NULL) report_error("%s error: `%s` with an empty stack", loc_to_str(&locid(ast->loc_id)), op);
	return *type;
}

// Perform binary operation with rax, rbx
#define print_binop(...) do {	\
	wtln("pop rbx"); \
	wtln("mov rax, qword [rsp]"); \
	wtln(__VA_ARGS__); \
	wtln("mov [rsp], rax"); \
} while (0)

static void
rsp_stack_mov_rsp(void)
{
	wtln("mov rax, qword [rsp_stack_ptr]");
	wtln("mov qword [rax], rsp");
	wtln("add qword [rsp_stack_ptr], WORD_SIZE");
}

static void
rsp_stack_mov_to_rsp(void)
{
	wtln("sub qword [rsp_stack_ptr], WORD_SIZE");
	wtln("mov rax, qword [rsp_stack_ptr]");
	wtln("mov rsp, qword [rax]");
}

static void
compile_inline(Compiler *ctx, const ast_t *decl_ast, bool is_proc)
{
	// Save old proc/func context and reset the current one
	const void *old_stmt = NULL;
	size_t old_stack_size = 0;
	value_kind_t old_stack_types[FUNC_CTX_MAX_STACK_TYPES_CAP] = {0};
	if (is_proc) {
		old_stmt = ctx->proc_ctx.stmt;
		old_stack_size = ctx->proc_ctx.stack_size;
		memcpy(old_stack_types, ctx->proc_ctx.stack_types, sizeof(value_kind_t) * ctx->proc_ctx.stack_size);
		ctx->proc_ctx.stmt = &decl_ast->proc_stmt;
		ctx->proc_ctx.stack_size = 0;
	} else {
		old_stmt = ctx->func_ctx.stmt;
		old_stack_size = ctx->func_ctx.stack_size;
		memcpy(old_stack_types, ctx->func_ctx.stack_types, sizeof(value_kind_t) * ctx->func_ctx.stack_size);
		ctx->func_ctx.stmt = &decl_ast->func_stmt;
		ctx->func_ctx.stack_size = 0;
	}

	// Preserve old rsp
	rsp_stack_mov_rsp();

	ast_t ast = astid(is_proc ? decl_ast->proc_stmt.body : decl_ast->func_stmt.body);

	compile_block(ctx, ast);

	const value_kind_t *ret_types = is_proc ?
		NULL : decl_ast->func_stmt.ret_types;

	const size_t ret_types_count = is_proc ?
		0: vec_size(ret_types);

	for (size_t i = 0; i < ret_types_count; ++i) {
		wtprintln("pop %s", X86_64_LINUX_CONVENTION_REGISTERS[i]);
	}

	// Set rsp to the old rsp
	rsp_stack_mov_to_rsp();

	// Drop all the leftovers and push return values on the stack
	wtprintln("add rsp, %u", vec_size(is_proc ?
																		decl_ast->proc_stmt.args
																		: decl_ast->func_stmt.args) * WORD_SIZE);

	for (size_t i = 0; i < ret_types_count; ++i) {
		wtprintln("push %s", X86_64_LINUX_CONVENTION_REGISTERS[i]);
	}

	// Set  current context to the old one
	if (is_proc) {
		ctx->proc_ctx.stack_size = old_stack_size;
		memcpy(ctx->proc_ctx.stack_types, old_stack_types, sizeof(value_kind_t) * old_stack_size);
		ctx->proc_ctx.stmt = (const proc_stmt_t *) old_stmt;
	} else {
		ctx->func_ctx.stack_size = old_stack_size;
		memcpy(ctx->func_ctx.stack_types, old_stack_types, sizeof(value_kind_t) * old_stack_size);
		ctx->func_ctx.stmt = (const func_stmt_t *) old_stmt;
	}
}

static size_t arg_idx = 0;

static const arg_t *
check_for_arg(Compiler *ctx, const char *str)
{
	arg_idx = 0;
	arg_t *arg = NULL;

	arg_t *args = ctx->proc_ctx.stmt == NULL ?
		ctx->func_ctx.stmt->args
		: ctx->proc_ctx.stmt->args;

	FOREACH_IDX(idx, arg_t, arg_, args) {
		if (0 == strcmp(arg_.name, str)) {
			arg = &arg_;
			arg_idx = idx;
			break;
		}
	}

	return arg;
}

static void
compile_function_call(Compiler *ctx, i32 value_idx, const ast_t *ast)
{
	const size_t stack_size = get_stack_size(ctx);

	size_t args_count_required = 0;
	const value_t value = values_map[value_idx].value;
	const ast_t *decl_ast = &astid(values_map[value_idx].value.ast_id);
	if (value.ast_kind == AST_PROC) {
		args_count_required = vec_size(decl_ast->proc_stmt.args);
	} else if (value.ast_kind == AST_FUNC) {
		args_count_required = vec_size(decl_ast->func_stmt.args);
	} else {
		UNREACHABLE;
	}

	if (stack_size < args_count_required) {
		eprintf("%s error: stack underflow trying to call: `%s`\n",
						loc_to_str(&locid(ast->loc_id)), ast->call.str);

		report_error("	note: expected amount of values on "
								 "the stack: %zu, the actual stack size: %zu",
								 args_count_required, stack_size);
	}

	for (size_t i = 0; i < args_count_required; ++i) {
		value_kind_t expected = 0;
		value_kind_t got = *get_type_from_end(ctx, args_count_required - 1 - i);
		if (value.ast_kind == AST_PROC) {
#if defined(DEBUG) || defined(PRINT_STACK)
			printf("--- %s ---\n", decl_ast->proc_stmt.name->str);
#endif
			expected = decl_ast->proc_stmt.args[i].kind;
		} else {
#if defined(DEBUG) || defined(PRINT_STACK)
			printf("--- %s ---\n", decl_ast->func_stmt.name->str);
#endif
			expected = decl_ast->func_stmt.args[i].kind;
		}

#if defined(DEBUG) || defined(PRINT_STACK)
		stack_dump(ctx);
#endif

		if (expected != got) {
			loc_id_t arg_loc = 0;
			if (ast->ast_id - 1 > 0
			&& astid(ast->ast_id - 1).ast_kind == AST_PUSH)
			{
				arg_loc = astid(ast->ast_id - 1).loc_id;
			} else if (value.ast_kind == AST_PROC) {
				arg_loc = decl_ast->proc_stmt.args[i].loc_id;
			} else {
				arg_loc = decl_ast->func_stmt.args[i].loc_id;
			}

			eprintf("%s error: expected %zuth argument to call `%s` to be `%s`, but "
							"got: `%s`\n",
							loc_to_str(&locid(arg_loc)), i, ast->call.str,
							value_kind_to_str_pretty(expected),
							value_kind_to_str_pretty(got));

			report_error("%s note: `%s` defined here",
									 loc_to_str(&locid(decl_ast->loc_id)), ast->call.str);
		}
	}

	for (size_t i = 0; i < args_count_required; ++i) {
		stack_pop(ctx);
	}

	if (value.ast_kind == AST_PROC && decl_ast->proc_stmt.body >= 0) {
		if (decl_ast->proc_stmt.inlin) {
			compile_inline(ctx, decl_ast, true);
		} else {
			wtprintln("call __%s__", decl_ast->proc_stmt.name->str);
		}
	} else {
		if (decl_ast->func_stmt.inlin && decl_ast->func_stmt.body >= 0) {
			compile_inline(ctx, decl_ast, false);
		} else {
			wtprintln("call __%s__", decl_ast->func_stmt.name->str);
		}

		// Add return values from function to the types stack
		for (size_t i = vec_size(decl_ast->func_stmt.ret_types); i > 0; --i) {
			stack_add_type(ctx, decl_ast->func_stmt.ret_types[i - 1]);
		}
	}

	const value_t value_ = {
			.ast_id = value.ast_id,
			.ast_kind = value.ast_kind,
			.is_used = true
	};

	shput(values_map, ast->call.str, value_);
}

// TODO: deprecate `funcptr` type.
// It may happen when you have a function that accepts `funcptr` as an argument,
//   and you trying to call this argument, which is basically just calling a function pointer.
static void
compile_argument_push_or_call(Compiler *ctx, i32 value_idx, const ast_t *ast, bool is_call)
{
	if (ctx->proc_ctx.stmt != NULL || ctx->func_ctx.stmt != NULL) {
		const arg_t *arg = check_for_arg(ctx, ast->literal.str);
		if (arg == NULL) {
			report_error("%s error: undefined symbol: `%s`",
									 loc_to_str(&locid(ast->loc_id)), ast->literal.str);
		}

		// Compute the index of the value from the end of the stack
		size_t stack_idx = 0;
		if (ctx->proc_ctx.stmt != NULL) {
			stack_idx = vec_size(ctx->proc_ctx.stmt->args) - arg_idx + ctx->proc_ctx.stack_size;
			if (ctx->proc_ctx.stmt->inlin) stack_idx--;
		} else {
			stack_idx = vec_size(ctx->func_ctx.stmt->args) - arg_idx + ctx->func_ctx.stack_size;
			if (ctx->func_ctx.stmt->inlin) stack_idx--;
		}

		if (is_call) {
			wtprintln("call qword [rsp + %zu]", stack_idx * WORD_SIZE);
		} else {
			wtprintln("mov rax, [rsp + %zu]", stack_idx * WORD_SIZE);
			wtln("push rax");
			stack_add_type(ctx, arg->kind);
		}
	} else {
		if (value_idx == -1) {
			report_error("%s error: undefined symbol: `%s`",
									 loc_to_str(&locid(ast->loc_id)), ast->call.str);
		}
	}
}

static void
compile_ast(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	if (ast->ast_kind != AST_PROC && ast->ast_kind != AST_FUNC) {
		wprintln("; -- %s --", ast_kind_to_str(ast->ast_kind));

#ifdef PRINT_STACK
		printf("%s:\n", ast_kind_to_str(ast->ast_kind));
		stack_dump(ctx);
#endif
	}
#endif

	switch (ast->ast_kind) {
	// These are handled in different place
	case AST_VAR:
	case AST_FUNC:
	case AST_CONST:
	case AST_PROC: {} break;

	case AST_IF: {
		check_for_integer_on_the_stack(ctx, "if",
																	 "expected last value on "
																	 "the stack to be integer",
																	 ast);

		wtln("pop rax");
		stack_pop(ctx);

		// If statement is empty
		if (ast->if_stmt.then_body < 0 && ast->if_stmt.else_body < 0) return;

		const size_t curr_label = label_counter++;

		wtln("test rax, rax");
		wtprintln("jz ._else_%zu", curr_label);

		ast_t if_ast = astid(ast->if_stmt.then_body);
		if (ast->if_stmt.then_body >= 0) {
			compile_block(ctx, if_ast);
			wtprintln("jmp ._edon_%zu", curr_label);
		}

		wprintln("._else_%zu:", curr_label);
		if (ast->if_stmt.else_body >= 0) {
			if_ast = astid(ast->if_stmt.else_body);
			compile_block(ctx, if_ast);
		}

		wprintln("._edon_%zu:", curr_label);
	} break;

	case AST_WRITE: {
		value_kind_t value_kind = check_stack_for_last(ctx, "write", ast);
		const char *str = ast->write_stmt.token->str + 1;
		const i32 var_idx = shgeti(ctx->var_map, str);
		if (var_idx == -1) {
			report_error("%s error: undefined symbol: `%s`",
									 loc_to_str(&locid(ast->loc_id)),
									 str);
		}

		const consteval_value_t value = ctx->var_map[var_idx].value;

		if (value_kind != value.kind) {
			report_error("%s error: write a value of type `%s` into the variable of type `%s`",
									 loc_to_str(&locid(ast->loc_id)),
									 value_kind_to_str_pretty(value_kind),
									 value_kind_to_str_pretty(value.kind));
		}

		wtln("pop rax");
		wtprintln("mov [%s], rax", str);
		stack_pop(ctx);
	} break;

	case AST_SYSCALL: {
		const size_t stack_size = get_stack_size(ctx);
		if (stack_size < ast->syscall.args_count + 1) {
			report_error("%s error: too few arguments to call: `syscall%d`",
									 loc_to_str(&locid(ast->loc_id)),
									 ast->syscall.args_count);
		}

		// Pop syscall number
		wtln("pop rax");

		// Pop all the shit in the reversed order
		for (u8 i = 0; i < ast->syscall.args_count; ++i) {
			wtprintln("pop %s", X86_64_LINUX_CONVENTION_REGISTERS[ast->syscall.args_count - 1 - i]);
		}

		wtln("syscall");
	} break;

	case AST_WHILE: {
		const size_t curr_label = while_label_counter++;

		wprintln("._while_%zu:", curr_label);

#ifdef DEBUG
		wln("; -- COND --");
#endif

		ast_t while_ast;
		ast_id_t last_ast_id_in_body;

		const size_t start_stack_size = get_stack_size(ctx);

		if (ast->while_stmt.cond < 0) goto compile_loop;

		while_ast = astid(ast->while_stmt.cond);
		if (ast->while_stmt.cond >= 0) {
			last_ast_id_in_body = compile_block(ctx, while_ast);
		}

#ifdef DEBUG
		wln("; -- COND END --");
#endif

		check_for_integer_on_the_stack(ctx, "while",
																	 "expected last value after performing `while` "
																	 "condition on the stack to be integer",
																	 ast);

		wtln("pop rax");
		stack_pop(ctx);
		wtln("test rax, rax");

		wtprintln("jz ._wdon_%zu", curr_label);

compile_loop:

		if (ast->while_stmt.body >= 0) {
			while_ast = astid(ast->while_stmt.body);
			last_ast_id_in_body = compile_block(ctx, while_ast);
		}

		const size_t end_stack_size = get_stack_size(ctx);

		/*
			We do check here only if:
				While statement is not in procedure OR while statement is in procedure/function, but you didn't call a function pointer in it,
				because, if you did, we won't be able to keep track of the stack.
		*/
		if (start_stack_size != end_stack_size
			&& !((ctx->proc_ctx.stmt == NULL || !ctx->proc_ctx.called_funcptr)
				|| (ctx->func_ctx.stmt == NULL || !ctx->func_ctx.called_funcptr)))
		{
			eprintf("%s error: The amount of elements at the start of the `while` statement "
							"should be equal to the amount of elements at the end of the statement\n",
							loc_to_str(&locid(ast->loc_id)));

			eprintf("  note: expected size: %zu, but got: %zu. Perhaps, %s\n",
							start_stack_size,
							end_stack_size,
							end_stack_size > start_stack_size ?
							"you can drop some elements" : "you lost the counter"
			);

			report_error("%s end of the statement", loc_to_str(&locid(astid(last_ast_id_in_body).loc_id)));
		}

		wtprintln("jmp ._while_%zu", curr_label);
		wprintln("._wdon_%zu:", curr_label);
	} break;

	case AST_LITERAL: {
		const i32 value_idx = shgeti(values_map, ast->literal.str);
		const i32 const_idx = shgeti(ctx->const_map, ast->literal.str);
		const i32 var_idx   = shgeti(ctx->var_map, ast->literal.str);
		if (const_idx != -1) {
			const consteval_value_t value = ctx->const_map[const_idx].value;
			wtprintln("push 0x%lX", value.value);
			stack_add_type(ctx, value.kind);
		} else if (var_idx != -1) {
			wtprintln("push [%s]", ast->literal.str);
			stack_add_type(ctx, ctx->const_map[const_idx].value.kind);
		} else if (value_idx != -1) {
			const value_t value = values_map[value_idx].value;
			if (value.ast_kind == AST_PROC) {
				wtprintln("push __%s__", astid(value.ast_id).proc_stmt.name->str);
			} else if (value.ast_kind == AST_FUNC) {
				wtprintln("push __%s__", astid(value.ast_id).func_stmt.name->str);
			} else {
				UNREACHABLE;
			}

			const value_t value_ = {
				.ast_id = value.ast_id,
				.ast_kind = value.ast_kind,
				.is_used = true
			};
			shput(values_map, ast->call.str, value_);
			stack_add_type(ctx, VALUE_KIND_FUNCTION_POINTER);
		} else {
			compile_argument_push_or_call(ctx, value_idx, ast, false);
		}
	} break;

	case AST_CALL: {
		const i32 value_idx = shgeti(values_map, ast->call.str);
		const i32 var_idx = shgeti(ctx->var_map, ast->call.str);
		if (value_idx != -1) {
			compile_function_call(ctx, value_idx, ast);
		} else if (var_idx != -1) {
			wtprintln("mov rax, [%s]", ctx->var_map[var_idx].key);
			wtln("push rax");
			stack_add_type(ctx, ctx->var_map[var_idx].value.kind);
		} else {
			compile_argument_push_or_call(ctx, value_idx, ast, false);
		}
	} break;

	case AST_PUSH: {
		switch (ast->push_stmt.value_kind) {
		case VALUE_KIND_INTEGER: {
			wtprintln("mov rax, 0x%lX", ast->push_stmt.integer);
			wtln("push rax");
			stack_add_type(ctx, VALUE_KIND_INTEGER);
		} break;

		case VALUE_KIND_STRING: {
			scratch_buffer_genstr();
			wtprintln("mov rax, %s", scratch_buffer_to_string());
			wtln("push rax");
			stack_add_type(ctx, VALUE_KIND_STRING);
			string_literal_counter++;
			vec_add(strs, ast->push_stmt.str);
		} break;

		case VALUE_KIND_BYTE: TODO break;

		case VALUE_KIND_FUNCTION_POINTER:
		case VALUE_KIND_POISONED:
		case VALUE_KIND_LAST:	UNREACHABLE; break;
		}
	} break;

	case AST_PLUS: {
		first_type = get_type_from_end(ctx, 0);
		second_type = get_type_from_end(ctx, 1);

		if (second_type == NULL || first_type == NULL) {
			report_error("%s error: `%s` stack underflow, bruv", loc_to_str(&locid(ast->loc_id)), "+");
		} else if ((*first_type  != VALUE_KIND_INTEGER
						 && *first_type  != VALUE_KIND_BYTE
						 && *first_type  != VALUE_KIND_STRING)
					 ||  (*second_type != VALUE_KIND_INTEGER
						 && *second_type != VALUE_KIND_BYTE
						 && *second_type != VALUE_KIND_STRING))
		{
			report_error("%s error: expected to have `int`, `byte` or `str`s on the stack, but got: `%s` and `%s`",
									 loc_to_str(&locid(ast->loc_id)),
									 value_kind_to_str_pretty(*second_type),
									 value_kind_to_str_pretty(*first_type));
		}

		if (*first_type == VALUE_KIND_INTEGER
		&& *second_type == VALUE_KIND_INTEGER)
		{
			print_binop("add rax, rbx");
			stack_pop(ctx);
			return;
		}

		switch (*first_type) {
		case VALUE_KIND_STRING:
		case VALUE_KIND_INTEGER: {
			wtln("pop rbx");
			wtln("mov rax, qword [rsp]");
			wtln("mov al, byte [rax + rbx]");
			wtln("movzx rax, al");
			wtln("mov [rsp], rax");
			stack_pop(ctx);
			*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_BYTE;
		} break;

		case VALUE_KIND_BYTE: TODO break;

		case VALUE_KIND_FUNCTION_POINTER:
		case VALUE_KIND_POISONED:
		case VALUE_KIND_LAST: UNREACHABLE;
		}
	} break;

	case AST_BNOT: {
		const value_kind_t *type = get_type_from_end(ctx, 0);

		if (type == NULL) {
			report_error("%s error: `%s` stack underflow, bruv", loc_to_str(&locid(ast->loc_id)), "+");
		} else if (*type  != VALUE_KIND_INTEGER && *type  != VALUE_KIND_BYTE) {
			report_error("%s error: expected to have a `byte` or an `int` on the stack, but got: `%s`",
									 loc_to_str(&locid(ast->loc_id)),
									 value_kind_to_str_pretty(*type));
		}

		wtln("mov rax, [rsp]");
		wtln("not rax");
		wtln("mov [rsp], rax");
	} break;

	case AST_BOR: {
		check_for_two_integers_on_the_stack(ctx, "|", ast);
		print_binop("or rax, rbx");
		stack_pop(ctx);
	} break;

	case AST_MINUS: {
		check_for_two_integers_on_the_stack(ctx, "-", ast);
		print_binop("sub rax, rbx");
		stack_pop(ctx);
	} break;

	case AST_DIV: {
		check_for_two_integers_on_the_stack(ctx, "/", ast);
		wtln("xor edx, edx");
		print_binop("div rbx");
		stack_pop(ctx);
	} break;

	case AST_MOD: {
		check_for_two_integers_on_the_stack(ctx, "%", ast);
		wtln("xor edx, edx");
		wtln("pop rbx");
		wtln("mov rax, qword [rsp]");
		wtln("div rbx");
		wtln("mov [rsp], rdx");
		stack_pop(ctx);
	} break;

	case AST_MUL: {
		check_for_two_integers_on_the_stack(ctx, "*", ast);
		wtln("xor edx, edx");
		print_binop("mul rbx");
		stack_pop(ctx);
	} break;

	case AST_EQUAL: {
		check_for_two_types_on_the_stack(ctx, "=", ast,
																		 VALUE_KIND_INTEGER, VALUE_KIND_BYTE,
																		 VALUE_KIND_INTEGER, VALUE_KIND_BYTE);

		wtln("pop rax");
		wtln("mov rbx, qword [rsp]");
		wtln("cmp rax, rbx");
		wtln("sete al");
		wtln("movzx rax, al");
		wtln("mov [rsp], rax");
		stack_pop(ctx);
		*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_INTEGER;
	} break;

	case AST_LESS: {
		check_for_two_integers_on_the_stack(ctx, "<", ast);
		wtln("pop rax");
		wtln("mov rbx, qword [rsp]");
		wtln("cmp rbx, rax");
		wtln("setb al");
		wtln("movzx rax, al");
		wtln("mov [rsp], rax");
		stack_pop(ctx);
		*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_INTEGER;
	} break;

	case AST_GREATER: {
		check_for_two_integers_on_the_stack(ctx, ">", ast);
		wtln("pop rax");
		wtln("mov rbx, qword [rsp]");
		wtln("cmp rbx, rax");
		wtln("setg al");
		wtln("movzx rax, al");
		wtln("mov [rsp], rax");
		stack_pop(ctx);
		*stack_at_mut(ctx, get_stack_size(ctx) - 1) = VALUE_KIND_INTEGER;
	} break;

	case AST_DROP: {
		check_stack_for_last(ctx, "drop", ast);
		wtln("pop rax");
		stack_pop(ctx);
	} break;

	case AST_DUP: {
		value_kind_t last_type = check_stack_for_last(ctx, "dup", ast);
		wtln("mov rax, [rsp]");
		wtln("push rax");
		stack_add_type(ctx, last_type);
	} break;

	case AST_DOT: {
		value_kind_t last_type = check_stack_for_last(ctx, ".", ast);
		switch (last_type) {
		// TODO: Create a separate function `dmp_byte`
		case VALUE_KIND_BYTE:
		case VALUE_KIND_FUNCTION_POINTER:
		case VALUE_KIND_INTEGER: {
			wtln("mov rax, qword [rsp]");
			wtln("mov r14, 0x1"); // mov 1 to r14 to print newline
			wtln("call dmp_i64");
			used_dmp_i64 = true;
		} break;

		case VALUE_KIND_STRING: {
			wtln("mov rdi, [rsp]");
			wtln("call strlen");
			wtln("mov rdx, rax");
			wtln("mov rsi, rdi");
			wtln("mov rax, SYS_WRITE");
			wtln("mov rdi, SYS_STDOUT");
			wtln("syscall");
			used_strlen = true;
		} break;

		case VALUE_KIND_POISONED: UNREACHABLE; break;
		case VALUE_KIND_LAST:			UNREACHABLE; break;
		}
	} break;

	case AST_POISONED: UNREACHABLE;
	}
}

INLINE void
print_defines(void)
{
	wln(DEFINE " SYS_WRITE  1");
	wln(DEFINE " SYS_STDOUT 1");
	wln(DEFINE " WORD_SIZE  8");
	wln(DEFINE " SYS_EXIT   60");
}

static void
print_dmp_i64(void)
{
#ifdef DEBUG
	wln("; r14b: newline");
	wln("; rax: number to print");
#endif
	wln("dmp_i64:");
	wtln("enter   0, 0");
	wtln("sub     rsp, 64");
	wtln("mov     qword [rbp - 8],  rax");
	wtln("mov     dword [rbp - 36], 0");
	wtln("mov     dword [rbp - 40], 0");
	wtln("cmp     qword [rbp - 8],  0");
	wtln("jge     .LBB0_2");
	wtln("mov     dword [rbp - 40], 1");
	wtln("xor     eax, eax");
	wtln("sub     rax, qword [rbp - 8]");
	wtln("mov     qword [rbp - 8], rax");
	wln(".LBB0_2:");
	wtln("cmp     qword [rbp - 8], 0");
	wtln("jne     .LBB0_4");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     ecx, eax");
	wtln("add     ecx, 1");
	wtln("mov     dword [rbp - 36], ecx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], 48");
	wtln("jmp     .LBB0_8");
	wln(".LBB0_4:");
	wtln("jmp     .LBB0_5");
	wln(".LBB0_5:");
	wtln("cmp     qword [rbp - 8], 0");
	wtln("jle     .LBB0_7");
	wtln("mov     rax, qword [rbp - 8]");
	wtln("mov     ecx, 10");
	wtln("cqo");
	wtln("idiv    rcx");
	wtln("mov     eax, edx");
	wtln("mov     dword [rbp - 44], eax");
	wtln("mov     eax, dword [rbp - 44]");
	wtln("add     eax, 48");
	wtln("mov     cl, al");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     edx, eax");
	wtln("add     edx, 1");
	wtln("mov     dword [rbp - 36], edx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], cl");
	wtln("mov     rax, qword [rbp - 8]");
	wtln("mov     ecx, 10");
	wtln("cqo");
	wtln("idiv    rcx");
	wtln("mov     qword [rbp - 8], rax");
	wtln("jmp     .LBB0_5");
	wln(".LBB0_7:");
	wtln("jmp     .LBB0_8");
	wln(".LBB0_8:");
	wtln("cmp     dword [rbp - 40], 0");
	wtln("je      .LBB0_10");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     ecx, eax");
	wtln("add     ecx, 1");
	wtln("mov     dword [rbp - 36], ecx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], 45");
	wln(".LBB0_10:");
	wtln("movsxd  rax, dword [rbp - 36]");
	wtln("mov     byte [rbp + rax - 32], 0");
	wtln("mov     dword [rbp - 48], 0");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("sub     eax, 1");
	wtln("mov     dword [rbp - 52], eax");
	wln(".LBB0_11:");
	wtln("mov     eax, dword [rbp - 48]");
	wtln("cmp     eax, dword [rbp - 52]");
	wtln("jge     .LBB0_13");
	wtln("movsxd  rax, dword [rbp - 48]");
	wtln("mov     al, byte [rbp + rax - 32]");
	wtln("mov     byte [rbp - 53], al");
	wtln("movsxd  rax, dword [rbp - 52]");
	wtln("mov     cl, byte [rbp + rax - 32]");
	wtln("movsxd  rax, dword [rbp - 48]");
	wtln("mov     byte [rbp + rax - 32], cl");
	wtln("mov     cl, byte [rbp - 53]");
	wtln("movsxd  rax, dword [rbp - 52]");
	wtln("mov     byte [rbp + rax - 32], cl");
	wtln("mov     eax, dword [rbp - 48]");
	wtln("add     eax, 1");
	wtln("mov     dword [rbp - 48], eax");
	wtln("mov     eax, dword [rbp - 52]");
	wtln("add     eax, -1");
	wtln("mov     dword [rbp - 52], eax");
	wtln("jmp     .LBB0_11");
	wln(".LBB0_13:");
	wtln("mov     eax, dword [rbp - 36]");
	wtln("mov     ecx, eax");
	wtln("add     ecx, 1");
	wtln("mov     dword [rbp - 36], ecx");
	wtln("cdqe");
	wtln("mov     byte [rbp + rax - 32], 10");
	wtln("lea     rsi, [rbp - 32]");
	wtln("movsxd  rdx, dword [rbp - 36]");
	wtln("test    r14b, r14b");
	wtln("jz      .not_newline");
	wtln("jmp     .write");
	wln(".not_newline:");
	wtln("dec rdx");
	wln(".write:");
	wtln("mov     eax, SYS_WRITE");
	wtln("mov     edi, SYS_STDOUT");
	wtln("syscall");
	wtln("add     rsp, 64");
	wtln("leave");
	wtln("ret");
}

static void
print_strlen(void)
{
	wln("strlen:");
	wtln("xor rax, rax");
	wln(".loop:");
	wtln("cmp byte [rdi + rax], 0");
	wtln("jz .done");
	wtln("inc rax");
	wtln("jmp .loop");
	wln(".done:");
	wtln("ret");
}

INLINE void
print_exit(void)
{
	wtln("mov rax, SYS_EXIT");
	wtln("mov rdi, [ret_code]");
	wtln("syscall");
}

INLINE void
print_data_section(void)
{
	wln(SECTION_BSS_WRITEABLE);
	wln("rsp_stack: " RESERVE_QUAD_WORD " 1024");
	wln(SECTION_DATA_WRITEABLE);
	wln("ret_code dq 0x0");
	wln("rsp_stack_ptr: dq rsp_stack");
}

Compiler
new_compiler(ast_id_t ast_cur,
						 const_map_t *const_map,
						 var_map_t *var_map)
{
	return (Compiler) {
		.ast_cur = ast_cur,
		.proc_ctx = {
			.stmt = NULL,
			.stack_size = 0,
			.called_funcptr = false
		},
		.var_map = var_map,
		.const_map = const_map,
	};
}

static void
compiler_deinit(void)
{
	shfree(values_map);
	fclose(stream);
}

static void
compiler_emergency_clean(void)
{
	compiler_deinit();
	main_deinit();
	remove(X86_64_OUTPUT);
}

static void
compile_proc(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	FOREACH(arg_t, arg, ast->proc_stmt.args) {
		wprintln("; %s", arg_to_str(&arg));
	}
#endif

	wprintln("__%s__:", ast->proc_stmt.name->str);
	rsp_stack_mov_rsp();

	ctx->proc_ctx.stmt = &ast->proc_stmt;

	ast_t proc_ast = astid(ast->proc_stmt.body);
	if (ast->proc_stmt.body >= 0) {
		compile_block(ctx, proc_ast);
	}

	rsp_stack_mov_to_rsp();

	// Pop return address into the rax
	wtln("pop rax");

	// Pop the amount of procedure's arguments out of the stack
	wtprintln("add rsp, %u", (vec_size(ast->proc_stmt.args)) * WORD_SIZE);

	// Push return address from rax and return
	wtln("push rax");
	wtln("ret");

	ctx->proc_ctx.stmt = NULL;
	ctx->proc_ctx.stack_size = 0;
	ctx->proc_ctx.called_funcptr = false;
}

static void
compile_func(Compiler *ctx, const ast_t *ast)
{
#ifdef DEBUG
	FOREACH(arg_t, arg, ast->func_stmt.args) {
		wprintln("; %s", arg_to_str(&arg));
	}
#endif

	wprintln("__%s__:", ast->func_stmt.name->str);
	rsp_stack_mov_rsp();

	ctx->func_ctx.stmt = &ast->func_stmt;

	ast_id_t last_ast_in_body = ast->ast_id;
	ast_t func_ast = astid(ast->func_stmt.body);
	if (ast->func_stmt.body >= 0) {
		last_ast_in_body = compile_block(ctx, func_ast);
	}

	const size_t ret_types_count = vec_size(ast->func_stmt.ret_types);
	if (ctx->func_ctx.stack_size < ret_types_count) {
		report_error("%s error: expected to have %zu %s on the stack "
								 "to perform implicit return function, but got only %zu",
								 loc_to_str(&locid(astid(last_ast_in_body).loc_id)),
								 ret_types_count,
								 ret_types_count == 1 ? "element" : "elements",
								 ctx->func_ctx.stack_size);
	}

	// Check if type of the argument in function signature matches
	// to the corresponding value's type on the stack.
	for (size_t i = 0; i < ret_types_count; ++i) {
		value_kind_t expected = ast->func_stmt.ret_types[i];
		value_kind_t got = *get_type_from_end(ctx, ret_types_count - 1 - i);
		if (expected != got) {
			if (i == 0) {
				report_error("%s error: expected last element on the stack to be `%s`, but got: `%s`",
								loc_to_str(&locid(astid(last_ast_in_body).loc_id)),
								value_kind_to_str_pretty(got),
								value_kind_to_str_pretty(expected));
			} else {
				report_error("%s error: expected %zuth element from the end of the stack to be `%s`, but got: `%s`",
										 loc_to_str(&locid(astid(last_ast_in_body).loc_id)),
										 i,
										 value_kind_to_str_pretty(got),
										 value_kind_to_str_pretty(expected));
			}
		}
	}

	// Save return values into the registers
	for (size_t i = 0; i < ret_types_count; ++i) {
		wtprintln("pop %s", X86_64_LINUX_CONVENTION_REGISTERS[i]);
	}

	// If name of the function is `main`, save last returned value to the `ret_code`,
	// to use it as an exit code in the future.
	if (0 == strcmp(MAIN_FUNCTION, ctx->func_ctx.stmt->name->str)) {
		wtprintln("mov [ret_code], %s", X86_64_LINUX_CONVENTION_REGISTERS[ret_types_count - 1]);
	}

	rsp_stack_mov_to_rsp();

	// Pop return address into the rax
	wtln("pop rax");

	// Pop the amount of function's arguments out of the stack
	wtprintln("add rsp, %u", (vec_size(ast->func_stmt.args)) * WORD_SIZE);

	// Retrieve return values
	for (size_t i = 0; i < ret_types_count; ++i) {
		wtprintln("push %s", X86_64_LINUX_CONVENTION_REGISTERS[i]);
	}

	// Push return address from rax and return
	wtln("push rax");
	wtln("ret");

	ctx->func_ctx.stmt = NULL;
	ctx->func_ctx.stack_size = 0;
	ctx->func_ctx.called_funcptr = false;
}

static void
report_if_redeclared(Compiler *ctx, const ast_t *ast, const char *key)
{
	ast_id_t ast_id = -1;

	if (-1 != shgeti(values_map, key)) {
		ast_id = values_map[shgeti(values_map, key)].value.ast_id;
	} else if (-1 != shgeti(ctx->const_map, key)) {
		ast_id = ctx->const_map[shgeti(ctx->const_map, key)].value.ast_id;
	} else if (-1 != shgeti(ctx->var_map, key)) {
		ast_id = ctx->var_map[shgeti(ctx->var_map, key)].value.ast_id;
	}

	if (-1 != ast_id) {
		eprintf("%s error: %s `%s` got redeclared\n",
						loc_to_str(&locid(ast->loc_id)),
						ast_kind_to_str_pretty(ast->ast_kind),
						key);

		report_error("%s note: previously declared here",
								 loc_to_str(&locid(astid(ast_id).loc_id)));
	}
}

static void
fill_values_map(Compiler *ctx)
{
	ast_t ast = astid(0);
	while (ast.next && ast.next <= ASTS_SIZE) {
		switch (ast.ast_kind) {
		case AST_POISONED:
		case AST_IF:
		case AST_WHILE:
		case AST_DOT:
		case AST_DUP:
		case AST_BNOT:
		case AST_BOR:
		case AST_MOD:
		case AST_PUSH:
		case AST_MUL:
		case AST_DIV:
		case AST_MINUS:
		case AST_PLUS:
		case AST_LESS:
		case AST_EQUAL:
		case AST_CALL:
		case AST_WRITE:
		case AST_DROP:
		case AST_GREATER:
		case AST_SYSCALL:
		case AST_LITERAL: break;

		case AST_VAR: {
			report_if_redeclared(ctx, &ast, ast.var_stmt.name->str);
		} break;

		case AST_CONST: {
			report_if_redeclared(ctx, &ast, ast.const_stmt.name->str);
		} break;

		case AST_FUNC:
		case AST_PROC: {
			const bool is_proc = ast.ast_kind == AST_PROC;
			const char *name = is_proc ? ast.proc_stmt.name->str : ast.func_stmt.name->str;
			report_if_redeclared(ctx, &ast, name);
			const value_t value = {
				.ast_id = ast.ast_id,
				.ast_kind = ast.ast_kind
			};
			shput(values_map, name, value);
		} break;
		}

		ast = astid(ast.next);
	}
}

static void
compile_comptime_string_literals(void)
{
	string_literal_counter = 0;
	FOREACH(const char *, str, strs) {
		scratch_buffer_clear();
		scratch_buffer_genstr();
		char *strdb = scratch_buffer_copy();
		size_t len = strlen(str);
		if (len > 2 && str[len - 3] == '\\' && str[len - 2] == 'n') {
			scratch_buffer_clear();
			scratch_buffer_append(str);

			scratch_buffer.str[len - 3] = '"';
			scratch_buffer.str[len - 2] = '\0';

			wprintln("%s db %s, 0xA, 0x0", strdb, scratch_buffer_to_string());
		} else {
			wprintln("%s db %s, 0x0", strdb, str);
		}

		scratch_buffer_genstrlen();
		wprintln("%s " COMPTIME_EQU " $ - %s", scratch_buffer_to_string(), strdb);
		string_literal_counter++;
	}
}

// TODO: Properly check if procedure/function is used or not
// Compile only used procs/funcs that are not inlined
static void
compile_funcs_and_procs(Compiler *ctx)
{
	for (ptrdiff_t i = 0; i < shlen(values_map); ++i) {
		const value_t value = values_map[i].value;
		if (value.ast_kind == AST_PROC
		&& !astid(value.ast_id).proc_stmt.inlin
		// && value.is_used
			)
		{
			compile_proc(ctx, &astid(value.ast_id));
		} else if (value.ast_kind == AST_FUNC
					 && !astid(value.ast_id).func_stmt.inlin
					 && 0 != strcmp(MAIN_FUNCTION, astid(value.ast_id).func_stmt.name->str)
					 // && value.is_used
			)
		{
			compile_func(ctx, &astid(value.ast_id));
		}
	}
}

void
compiler_compile(Compiler *ctx)
{
	stream = fopen(X86_64_OUTPUT, "w");
	if (stream == NULL) {
		eprintf("error: Failed to open file: %s\n", X86_64_OUTPUT);
		exit(EXIT_FAILURE);
	}

	wln(FORMAT_64BIT);

	print_defines();
	wln(SECTION_TEXT_EXECUTABLE);

	fill_values_map(ctx);

	ast_t ast = astid(ctx->ast_cur);
	compile_func(ctx, &ast);

	wln(GLOBAL " _start");
	wln("_start:");

	wtln("call __" MAIN_FUNCTION "__");

	print_exit();

	compile_funcs_and_procs(ctx);

	if (used_dmp_i64) print_dmp_i64();
	if (used_strlen) print_strlen();

	print_data_section();

	for (ptrdiff_t i = 0; i < shlen(ctx->var_map); ++i) {
		wprintln("%s dq 0x%lX", ctx->var_map[i].key, ctx->var_map[i].value.value);
	}

	compile_comptime_string_literals();
	compiler_deinit();
}

/* TODO:
	#3. Distinguish between compile-time strings and runtime strings, to reduce the amount of calls to strlen
	#4. Introduce let-binding notion
	#5. Introduce `elif` keyword
*/
