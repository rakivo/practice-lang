# EXTREMELY IMPORTANT! THIS LANGUAGE IS A WORK IN PROGRESS! ANYTHING CAN CHANGE AT ANY MOMENT WITHOUT ANY NOTICE! USE THIS LANGUAGE AT YOUR OWN RISK!

## `prac` - An Esoteric Concatenative Stack Based Statically Typed Computer Programming Language

# Super simple example of calling raylib from [`prac` Programming Language](https://github.com/rakivo/practice-lang)
> You can take a closer look at the example here: <https://github.com/rakivo/practice-lang/blob/master/prac/raylib.prac>
```ruby
include "prac/raylib_bindings.prac"

const KEY_Q 81                       end
const FONT_SIZE 50                   end
const TEXT_COLOR 4294967295          end
const BACKGROUND_COLOR 1095620892696 end

inline func get_text_x int
do
  GetScreenWidth! 2 / FONT_SIZE 3 * -
end

inline func get_text_y int
do
  GetScreenHeight! 2 / FONT_SIZE -
end

func main int do
  800 600 "hello\n" InitWindow!

  KEY_Q SetExitKey!

  while WindowShouldClose! 0 = do
    BeginDrawing!
      BACKGROUND_COLOR ClearBackground!
      "hello world"
      get_text_x!
      get_text_y!
      FONT_SIZE
      TEXT_COLOR
      DrawText!
    EndDrawing!
  end

  CloseWindow!

  0
end
```
> To run the example (you also need to have [raylib](https://github.com/raysan5/raylib/) install on your machine):
```console
$ make
$ ./build/pracc ./prac/raylib.prac
$ make link_with_raylib
$ ./out
```

## Functions in the [`prac` Programming Language](https://github.com/rakivo/practice-lang)

#### An example of declaring and defining a function:
```
func main int do
  0
end
```
> As you can see, the procedure does not accept any arguments and returns an integer. And, you've just seen the definition of the `main` function, which is mandatory for all [`prac-lang`](https://github.com/rakivo/practice-lang) programs.

> Function which accepts an integer and returns its square:
```ruby
func square int
  int x
do
  x x *
end
```

> If you've read the article about [`Procedures in the prac Programming Language`](https://github.com/rakivo/practice-lang/procs.md), the syntax may seem pretty familiar. A declaration of a function differs from a declaration of a procedure just by an additional type specification after the name of the function, and, by the keyword, indeed. (use `func` instead of `proc`)

#### What about manipulations with the stack?
> Basically, it's just the same as in procedures. Functions do not have access to the global stack, which basically means, that you can manipulate with the global stack only by accepting arguments and returning a value.

#### Function that returns many values
> I added this feature yesterday (2024-09-06), the syntax is so simple, just look at this function's signature:
```
func swap int int
  int a
  int b
do
  b a
end
```
> If you wanna return more than one value from your function, you just add an additional type after the first one, which is mandatory for functions, as I already said. If you wanna have a function that returns `void`, you can read this topic: [`Procedures in the prac Programming Language`](https://github.com/rakivo/practice-lang/procs.md).

# [Procedures in the `prac` Programming Language](https://github.com/rakivo/practice-lang/procs.md)

#### An example of declaring and defining a procedure:
```ruby
proc hello do
  "hello world\n" . drop
end
```
> As you can see, the procedure does not accept any arguments, you can see how to accept them in the next snippet.

```ruby
proc print_sum
  int a
  int b
do
  a b + .
end
```
> Here you can see an example of defining a procedure that accepts two arguments, sums them up and prints them.

#### What about manipulations with the stack?
> Essentially, procedures in the language do not have access to the global stack, which means that you can manipulate with the global stack only by accepting arguments.

##### But what happens if you have remaining values at the end of the procedure, like in this procedure:
```ruby
proc foo
  int a
  int b
do
  a b a b dup dup
end
```
> Basically, all the leftovers will be dropped at the end of the procedure automatically, so it won't corrupt the stack
