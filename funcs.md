## Functions in the [`prac` Programming Language](https://github.com/rakivo/practice-lang)

#### An example of declaring and defining a function:
```
func main int do
  0
end
```
> As you can see, the procedure does not accept any arguments and returns an integer. And, you've just seen the definition of the `main` function, which is mandatory for all [`prac-lang`](https://github.com/rakivo/practice-lang) programs.

> Function which accepts an integer and returns its square:
```
func square int
  int x
do
  x x *
end
```

> If you've read the article about [`Procedures in the prac Programming Language`](https://github.com/rakivo/practice-lang/procs.md), the syntax may seem pretty familiar. A declaration of a function differs from a declaration of a procedure just by an additional type specification after the name of the function, and, by the keyword, indeed. (use `func` instead of `proc`)

#### What about manipulations with the stack?
> Basically, it's just the same as in procedures. Functions do not have access to the global stack, which basically means, that you can manipulate with the global stack only by accepting arguments and returning a value.
