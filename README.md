Yet an other `awk`-ish command line tool, with a
("point-free") functional approach.

- [Overview](#Overview)
  - [Getting Started](#Getting%20Started)
  - [Command Usage](#Command%20Usage)
- [Details](#Details)
  - [Syntax](#Syntax)
  - [Type System](#Type%20System)
    - [Coersion](#Coersion)
    - [Infinity](#Infinity)
  - [Operators](#Operators)
  - [Literals](#Literals)
  - [Prelude](#Prelude)
- [Library](#Library)

# Overview

Similarly to commands such as `awk`, `sed` or `perl
-pe`/`perl -ne`, `sel` takes a script to apply to its
standard input to produce its standard output.

## Getting Started

```console
$ meson setup build
$ ninja -C build
...
$ seq 17 42 | build/sel [...idk...]
17
19
23
29
31
37
41
```

## Command Usage

(flags (check, lookup/search, list, disable ic, ..))

---

# Details

The provided script defines an operation which is applied
on each line (by default, see [recurse]()). As such,
the input and output are strings of (Unicode) characters.
Note that the ending newline character (`\n` most of the
time) is removed before input and re-appended to the output
if not already present.

## Syntax

Comments span from the first `#` of a line to the end of
the line (`\n` character).

A script is an expression usually describing a function
from a string (eg. line) to another string.

A main idea behind the language it to not be botherd with
quoting when issuing a command through a POSIX shell. For
example, delimitating an expression (usually done with
`()`) here uses `[]`. Even though it seems this could
trigger file name expansion, this only occures if it
does not contain a space (and expressions within `[]`
will often contain spaces).

Operators are presented [here](#Operators). An exeptions
to the syntax is the `,` binary operator in that it is
the only one that is written with both its operand on
its sides.

The `,` operator is used to chain functions. It is
equivalent in Haskell to `flip (.)` or the Unix shell pipe
`|`. The function to the left is executed and its result
is passed to the function to the right.

Literals are presented [here](#Literals). Notably, strings
are delimited with `{` and `}` and negative numbers cannot
be expressed directly. As such, "negative 1" would have to
be expressed as eg. `[sub 0 1]`. There is also no way to
write a string literal containing an unmatched `{` or `}`.

Finally words are made of the 26 lower case letters from
a to z included. A word will always be looked up for its
value in the [prelude](#Prelude).

## Type System

The representation of types uses a subset of Haskell's
typing language.

The types are:
 - string (`Str`)
 - number (`Num`)
 - list (`[has]` eg. `[Str]` for list of strings)
 - function (for example `Num -> Str`)
 - couple, or tuple of size 2 (`(fst, snd)`)

Some examples of type definitions:
```hs
sum :: [Num] -> Num
map :: (a -> b) -> [a]* -> [b]*
```

In type definitions, lower case words mean the type may
be any, only known at run time.

### Coersion

<!--
Implicit coersion is provided for convenience. It is
attempted when an argument to a function does not match
the destination parameter. This behavior can be disabled
through some flag probably, in which case the same
situation will terminate with a type error. -->

Note the following implicit coersions:
 true type | destination  | behavior
-----------|--------------|----------
 `Num`     | `Str`        | writes the number in decimal
 `Str`     | `Num`        | tries to parse as a number
 `Str`     | `[Num]`      | list of the Unicode codepoints
 `Str`     | `[Str]`      | list of the Unicode graphemes
 `[Str]`   | `Str`        | joined with empty separator
<!--
 `Num`     | `(Num, Num)` | integer and fractional part -->

These rules apply recursively on complex data structures
(eg. `[Str]` to `[Num]`).

For example the two following scripts are effectively
equivalent (because the script input type is of `Str`
and its output is implicitly coersed back to `Str`):
```sh
seq 5 | sel tonum, map +1, tostr
seq 5 | sel map +1
```

Conditions (boolean values) may be represented with the
`Num` type. In that case 0 means false, any other value
means true.

### Infinity

Akin to Haskell, everything is lazy which enables working
with potentially infinite data structure without always
hanging.

A type affixed with a `*` means the value may not be
bounded (lists and strings). For example the following
function returns an infinite list:
```hs
repeat :: a -> [a]*
```

A function may specify that it can operate on such a data
structure by using the same notation:
```hs
join :: [a] -> [[a]]* -> [a]*
```

A value of finite type may be assigned to a parameter
marked infinite. The other way round is a type error.
For example calling `join` with the first argument of type
`Str*` is a type error.

Also a function keep the finite quality of its parameter to
its return. With the same `join` function as an example,
calling it with 2 arguments of finite types results in a
finite type value.
```hs
join "-" "abcd" :: Str -- = "a-b-c-d"
```

## Operators

A set of "binary" and "unary" operators is defined.
Although they are just aliases to functions in the prelude,
an operator will bind tighter to the next atom to make
a new single atom. These operators cannot appear without
one argument.

See the difference of interpretation in:
 script       | desugared
--------------|-----------
 `map sub 1`  | `map sub 1` (type error)
 `map %sub 1` | `map [flip sub] 1` (but still type error)
 `map -1`     | `map [[flip sub] 1]` (ie. x-1 for each x)
 `map %-1`    | `map [[flip [flip sub]] 1]` (ie. 1-x)

> If confused about the first 2 lines, remember that it
> could also be written as: `[map sub] 1`.

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
 `@`   | unary  | `` // (parse)
 `^`   |        | `` // ((reserved))
 `:`   |        | `` // ((reserved - maybe char literal))
 `~`   |        | `` // ((reserved - not safe as unary))

The difference between an unary operator and a binary
operator is that an unary operator will first check
if the next token is a binary operator. If it is one,
the unary operator is first applied the binary operator,
then the next atom is passed to the result. See the last
example in the table [above](#Operator).

Finally, it is important to notice that despite being
called "binary" operators, none can be used with infix
syntax as in `1 + 2` (this would become `1 [[flip add]
2]`). (Although the Polish/prefix notation is technically
correct: `+ 1 2`.)

## Literals

Numeric literals come in two forms: integers and floating
points. Both are expressed using the charaters `0` to `9`,
with `.` as the decimal separator. The floating point
notation needs the leading 0; `.5` will effectively be
desugared as `[flip mul] 5`. It is not possible to express
a negative value literally. To obtain such a value, the
`sub 0` function can be used.

String literals are written between matching `{` and
`}`. A string literal can contain matched pairs of these
characters; for example `{a {b} c}` is a valid string
contaning exactly `a {b} c`.

<!--
A character literal is a simpler way to represent a string
literal of a single character (grapheme). It is written by
prefixing the character with a `:`. Note that the character
can even be a space, so `: ` is the same as `{ }`. -->

There is no direct way to represent lists. To obtain a
list, the `@` operator (or [the underlying function]())
can be used. Here are some examples:
 expression                | yields (represented as JSON)
---------------------------|------------------------------
 `@{1, 2, 3}`              | [1, 2, 3]
 `@{1, 5:9}`               | [1, 5, 6, 7, 8]
 `@{1, 9:5}`               | [1, 9, 8, 7, 6]
 `@{this, is, some, text}` | ["this", "is", "some", "text"]
 `@{{1, 2}, {1:3}}`        | [[1, 2], [1, 2]]
 `@{text, 42, more}`       | ["text", "42", "more"]

As shown in the examples, when parsing an list, coersion
to `Num` is attempted for each entrie. If a single fails,
the list is kept as an list of `Str` (see also: [coersion
rules](#Coersion)).

## Prelude

(naming rules?)

# Library

---

### Work in Progress
#### Feature Goals
- lazy and infinite data structures
- lib interface
- broad prelude
- proper unicode (eg. graphems and word)
- REPL
- interactive (based on REPL)
