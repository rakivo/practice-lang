## Functions in the [`prac` Programming Language](https://github.com/rakivo/practice-lang)

#### An example of declaring and defining a function:
```ruby
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
```ruby
func swap int int
  int a
  int b
do
  b a
end
```
> If you wanna return more than one value from your function, you just add an additional type after the first one, which is mandatory for functions, as I already said. If you wanna have a function that returns `void`, you can read this topic: [`Procedures in the prac Programming Language`](https://github.com/rakivo/practice-lang/procs.md).
