# Type System

The representation of types uses a subset of Haskell's
typing language.

The types are:
 - string (`Str`)
 - number (`Num`)
 - list (`[has]` eg. `[Str]` for list of strings)
 - function (for example `Num -> Str`)
 - tuples (`(Num, Str)`) and pattern lists (`[Num, Str]`)

Some examples of type definitions:
```haskell
sum :: [Num] -> Num
map :: (a -> b) -> [a]* -> [b]*
```

In type definitions, lower case words mean the type may
be any, only known at run time (like `a` and `b` above).
For the meaning of the `*`, see [Infinity](#Infinity).

## Tuples and Pattern Lists

(todo)

tuples are backed by lists of known finite size (equal to how many types it has)

pattern lists are lists in wich a pattern is repeated (like unfolding a list of tuples)

none of this is properly implemented and used yet and thus subject to change

## Coersion

Implicit coersion is provided for convenience. It is
attempted when an argument to a function does not match
the destination parameter. This behavior can be disabled
with the `-s` ("strict types") flag, in which case any
coersion is invalid. In any case an invalid coersion is
a type error.

Note the following implicit coersions:
 true type | destination  | behavior
-----------|--------------|----------
 `Num`     | `Str`        | writes the number in decimal
 `Str`     | `Num`        | parse as a number, or `0`
 `Str`     | `[Num]`      | list of the Unicode codepoints
 `Str`     | `[Str]`      | list of the Unicode graphemes
 `[Str]`   | `Str`        | joined with empty separator

These rules apply recursively on complex data structures
(eg. `[Str]` to `[Num]` is valid). Multiple coersions may
happen at once.

For example the two following scripts are effectively
equivalent (because the script input type is of `Str`
and its output is implicitly coersed back to `Str`):
```sh
seq 5 | sel map +1
seq 5 | sel tonum, map +1, tostr
```

When the true type is `Str` and the destination type is
`[any]`, the coersion to a list of Unicode graphemes
is favored:
```sh
printf abc | sel reverse
printf abc | sel graphemes, reverse, join ::
```

Conditions (boolean values) may be represented with the
`Num` type. In that case 0 means false, any other value
means true.

## Infinity

(todo: yes it is implemented, but the typing is not
accounted for yet)

Akin to Haskell, everything is lazy which enables working
with potentially infinite data structure without always
hanging.

A type affixed with a `*` means the value may not be
bounded (lists and strings). For example the following
function returns an infinite list:
```haskell
repeat :: a -> [a]*
```

A function may specify that it can operate on such a data
structure by using the same notation:
```haskell
join :: Str -> [Str]* -> Str*
```

A value of finite type may be assigned to a parameter
marked infinite. The other way round is a type error.
For example calling `join` with the first argument of type
`Str*` is a type error.

Also a function keep the finite quality of its parameter to
its return. With the same `join` function as an example,
calling it with 2 arguments of finite types results in a
finite type value.
```haskell
join "-" "abcd" :: Str -- = "a-b-c-d"
```
