Yet an other `awk`-ish command line tool, with a
(point-free) functional approach.

- [Overview](#Overview)
  - [Getting Started](#Getting%20Started)
  - [Command Usage](#Command%Usage)
- [Details](#Details)
  - [Syntax](#Syntax)
  - [Type System](#Type%20System)
    - [Coersion](#Coersion)
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
$ cargo build --release
...
$ seq 17 42 | target/release/sel [...idk...]
17
19
23
29
31
37
41
```

## Command Usage

(flags (check, lookup/search, ..))

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

A script is an expression, written with few kinds of
tokens: operators, literals and words.

Operators are presented [here](#Operators). The exeptions
to the syntax are the following:
 syntax  | equivalent
---------|------------
 `a:b`   | `` // range a b
 `a, b`  | (no equivalent in the prelude) ((really?)) // then a b or [flip comp] a b (comp? circ? compose?)

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
 - array of string or number (`[Str]` and `[Num]`)
 - functions (for example `Num -> Str`)

Some examples of type definitions may look like the
following:
```hs
sum :: [Num] -> Num
map :: (a -> b) -> [a] -> [b]
```

In type definitions, lower case words mean the type may
be any, only known at run time.

### Coersion

Note the following implicit coersions:
 actual type | destination | behavior
-------------|-------------|----------
 `Num`       | `Str`       | writes the number in decimal
 `Str`       | `Num`       | tries to parse the string as a number
 `Str`       | `[Num]`     | array of the Unicode codepoints
 `Str`       | `[Str]`     | array of the Unicode graphemes

Coersion is attempted when an argument does not match
the destination parameter. For example the two following
scripts are effectively equivalent (because the script
input type is of `Str` and its output is coersed back to
`Str`):
```sh
seq 5 | sel tonum, map +1, tostr
seq 5 | sel map +1
```

Conditions (boolean values) may be represented with the
`Num` type. In that case 0 means false, any other value
means true.

## Operators

A set of "binary" and "unary" operators is defined.
Although this they are just aliases to functions in the
prelude, an operator will bind tighter to the next atom
to make a new atom.

See the difference of interpretation in:
 script      | desugared
-------------|-----------
 `map sub 1` | `map sub 1` (type error)
 `map -1`    | `map [[flip sub] 1]` (ie. x-1 for each x)
 `map %-1`   | `map [[flip [flip sub]] 1]` (ie. 1-x for each x)

The following tokens are used as operators:
 token | equivalent
-------|------------
 `+`   | `flip add`
 `-`   | `flip sub`
 `.`   | `flip mul`
 `/`   | `flip div`
 `=`   | `` // eq
 `%`   | `flip`
 `_`   | `index`
 `@`   | `` // (parse)
 `^`   | `` // (reserved)

## Literals

Numeric literals comes in two forms: integer and floating
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

There is no direct way to represent arrays. To obtain an
array, the `@` operator (or [the underlying function]())
can be used. Here are some examples:
 expression                | yields (represented as JSON)
---------------------------|------------------------------
 `@{1, 2, 3}`              | [1, 2, 3]
 `@{1, 5:9}`               | [1, 5, 6, 7, 8]
 `@{1, 9:5}`               | [1, 9, 8, 7, 6]
 `@{this, is, some, text}` | ["this", "is", "some", "text"]
 `@{{1, 2}, {1:3}}`        | [[1, 2], [1, 2]]
 `@{text, 42, more}`       | ["text", "42", "more"]

As shown in the last example, when parsing an array,
coersion to `Num` is attempted for every entries. If
a single fails, the array is kept as an array of `Str`
(see also: [coersion rules](#Coersion)).

## Prelude

(naming rules)

# Library

(features (unicode-segmentation))
