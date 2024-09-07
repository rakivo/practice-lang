" prac.vim - Syntax file for prac-lang

" Define keywords
syn keyword pracKeyword if func funcptr else while do proc end drop dup const int str syscall syscall1 syscall2 syscall3 syscall4 syscall5 syscall6 var bnot inline byte include

" Highlight keywords
hi def link pracKeyword Keyword

" Comment syntax
syn region pracComment start="# " end="$"
hi def link pracComment Comment

" Set the file type
let b:current_syntax = "prac"
