# Operators

A set of "binary" and "unary" operators is defined.
Although they are just aliases to builtin functions,
an operator will bind tighter to the next atom to make
a new single atom. These operators cannot appear without
their argument.

See the difference of interpretation in:
 script       | desugared
--------------|-----------
 `map sub 1`  | `map sub 1` (type error)
 `map %sub 1` | `map [flip sub] 1` (but still type error)
 `map -1`     | `map [[flip sub] 1]` (ie. x-1 for each x)
 `map %-1`    | `map [[flip [flip sub]] 1]` (ie. 1-x fe.)

> If confused about the first 2 lines being type errors,
> remember that it could also be written as: `[map sub] 1`.

The following tokens are used as operators:
 token | kind   | equivalent
-------|--------|------------
 `+`   | binary | `flip add`
 `-`   | binary | `flip sub`
 `.`   | binary | `flip mul`
 `/`   | binary | `flip div`
 `=`   | binary | `` // eq
 `_`   | binary | `index`
 `%`   | unary  | `flip`
 `@`   | unary  | `` // ((reserved))
 `^`   |        | `` // ((reserved - maybe about currying))
 `~`   |        | `` // ((reserved - not always safe character in shell))
 `!`   |        | `` // ((reserved - not always safe character in shell))

The difference between an unary operator and a binary
operator is that an unary operator will first check
if the next token is a binary operator. If it is one,
the unary operator is first applied the binary operator,
then the next atom is passed to the result. See the last
example in the [table above](#Operators).

Finally, it is important to notice that despite being
binary operators, none can be used with infix syntax as in
`1 + 2` (this would become `1 [[flip add] 2]`).

> The Polish/prefix notation is technically correct: `+ 1 2`.
