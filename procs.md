## Procedures in the [`prac` Programming Language](https://github.com/rakivo/practice-lang)

#### An example of declaring and defining a procedure:
```
proc hello do
  "hello world\n" . drop
end
```
> As you can see, the procedure does not accept any arguments, you can see how to accept them in the next snippet.

```
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
```
proc foo
  int a
  int b
do
  a b a b dup dup
end
```
> Basically, all the leftovers will be dropped at the end of the procedure automatically, so it won't corrupt the stack
