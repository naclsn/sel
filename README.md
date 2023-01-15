A versatile command line tool to deal with streams, with a
("point-free") functional approach.

> Global note that some things may be subject to changes.

---

##### State of Development

The project has be brought to a "usable" (~hackable) point.
However it is unfinished, notably:
- tests, I want more test, there is basically none
- infrastructure for tuples and pattern lists is in place, but incomplete
- same goes for affixing types with `*` (marks unbounded)
- (.. because of the previous on, literal lists are broken and crash type-checking)
- not every function that I would want are implemented, plan is to add as the need comes
- memory management is not quite a thing, this would mainly hinder use of `libsel.so`, with memory leaks
- (LLVM-based codegen is only a dream for now)

---

> (This file should probly be split into a wiki-ish set of pages.)

- [Overview](#Overview)
  - [Getting Started](#Getting%20Started)
    - [Setting Up for Development](Setting%20Up%20for%20Development)
  - [Command Usage](#Command%20Usage)
- [Details](#Details)
  - [Syntax](#Syntax)
  - [Type System](#Type%20System)
    - [Tuples and Pattern Lists](#Tuples%20and%20Pattern%20Lists)
    - [Coersion](#Coersion)
    - [Infinity](#Infinity)
  - [Operators](#Operators)
  - [Literals](#Literals)
  - [Builtins](#Builtins)
- [Library](#Library)
  - [Extending](#Extending)
    - [As a Builtin](#As%20a%20Builtin)
      - [Declaration](#Declaration)
      - [Definition](#Definition)
      - [Codegen](#Codegen)
    - [In the Prelude](#In%20the%20Prelude)

# Overview

Similarly to commands such as `awk` or `sed`, `sel` takes
a script to apply to its standard input to produce its
standard output.

## Getting Started

Project uses the meson build system. With ninja:
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

> sel: error while loading shared libraries: libsel.so:
> cannot open shared object file: No such file or directory

Note that on some distribution (eg. Fedora), meson will
install `libsel.so` under `/usr/local/lib{,64}`. If it is
not found at runtime, you may need to add it manually:
```console
# echo /usr/local/lib64 >/etc/ld.so.conf.d/sel.conf
# ldconfig # rebuild cache
```

### Setting Up for Development

To enable coverage:
```console
$ meson configure build -Db_coverage=true
$ pip install gcovr # for example
$ ninja -C build test # needed by meson
$ ninja -C build coverage
```

Due to the stupid length of some debug symbols, release
build are usually faster to compile:
```console
$ meson release --buildtype release
$ ninja -C release
```

## Command Usage

> The actual "front-end" is not fully designed yet.

```console
$ build/sel -h
Usage: build/sel [-Dns] <script...> | -f <file>
       build/sel -l [<names...>]
       build/sel [-s] -f <file> [-o <bin> <flags...>]
```

In the first form, `sel` runs a script that is read either
from the arguments (`<script..>` -- rest of the command
line) or from a file (with `-f`). When the script is given
through the command line, these are joined with a single
space (`' '`). From this script, an 'app' is made. This
app is then ran against the standard input to produce the
standard output.
- `-n` disables running the app, essentially only performing type-checking.
- `-s` can be used to enable strict typing (disallows auto-coersion).
- `-t` give the type of the given expression (if it is valid).
- `-D` does not execute the script but prints (unspecified) dev information.

The second form provides lookup and help for specified
names. When no names are given, every known names are
listed out (one by line).

With the last form, it is possible to make `sel` generate
a small executable which performs the provided script. It
is compiled leveraging LLVM, so it has to be available on
the system. This is not implemented yet.

---

# Details

In its most basic form, the script given to `sel` is a
serie of functions separated by `,` (comma). Each function
transforms its input and passes its output to the next
in line.

## Syntax

Comments span from the first `#` of a line to the end
of the line (`\n` character).

A main idea behind the language it to not be botherd with
quoting when issuing a command through a POSIX shell. For
example, delimitating an expression (usually done with
`()`) here uses `[]`. Even though it seems this could
trigger file name expansion, this only occures if it
does not contain a space (and expressions within `[]`
will often contain spaces).

Operators are presented [here](#Operators). Only the `,`
and `;` operators are ever written infix.

The `,` operator is used to chain functions. It is
equivalent in Haskell to `flip (.)` or the Unix shell pipe
`|`. The function to the left is executed and its result
is passed to the function to the right.

The `;` operator is not expected to be used by a short
script. It discards the value on the left and returns the
value on the right. Its main use is with the `def` syntax
(see below).

Literals are presented [here](#Literals). Notably, strings
are delimited within `:`s (which can be escaped as `::`)
and negative numbers cannot be expressed directly. As such,
"negative 1" would have to be expressed as eg. `[sub 0 1]`.
Lists are expressed using matching `{`-`}` (C-like).

Finally words are made of the 26 lower case letters from
`a` to `z` included. A word will always be looked up
for its value in the defined values first, then in the
[builtins](#Builtins).

To defined a new word: `def <docstring> <word>
<value>`. This will bind the value to the provided
word. Although it _is_ implemented in the parser as a
syntax, it can also be see as a (non-lazy) function:
```haskell
-- `name` must be an actual word, not a literal string
def :: Str -> Word -> a -> a
def docstring name value = value
```

Here is an example of how it might be used:
```
def hypot:
  compute the square root of the sum of the squares:
  [map sq, sum, sqrt]
  ;

# input would be a list of comma-separated numbers
split :,:, hypot
```

## Type System

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

### Tuples and Pattern Lists

(todo)

tuples are backed by lists of known finite size (equal to how many types it has)

pattern lists are lists in wich a pattern is repeated (like unfolding a list of tuples)

none of this is properly implemented and used yet and thus subject to change

### Coersion

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

### Infinity

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

## Operators

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

## Literals

Numeric literals are just as usual, exept the leading `0`
is needed for fractional numbers: `.5` will be parsed as
`[flip mul] 5` (see the [operators](#Operators)) so it
has to be written `0.5`.

For the same reason, it is not possible to express a
negative value literally. For this the `sub 0` function
can be used.

Support for hexadecimal, binary and octal notations can
be expected (at some point).

String literals are written between matching `:` and
`:`. To write a string containing the character `:`,
it can be doubled. The followig C-like escape sequences
are also supported: `\a`, `\b`, `\t`, `\n`, `\v`, `\f`,
`\r` and `\e`.

<!--
A character literal is a simpler way to represent a string
literal of a single character (grapheme). It is written by
prefixing the character with a `(idk yet)`. Note that the character
can even be a space, so `(idk yet) ` is the same as `: :`. -->

Lists are written as values within matching `{`-`}`,
separated by `,`. Thus for a list to contain a chain of
function separated by the `,` operator, it is needed to use
`[`-`]`. As in `{add 1, [sub 1, mul 5], sub 1}` is a list
of type `[Num -> Num]` and of length 3.

## Builtins

(naming rules?)
<!--
- functions names should come obvious from usual names in other programming  languages and a few rules (and common english - last resort)
- having multiple names perform the exact same operation is a plus for discoverability; this may ne preferable from introducing obscure, slight discrepencies between functions
- in that idea, it is ok if a name does not strictly follow the rules if it is easier to come up with
- not finding the exact name should not be too penalizing: there should be an other obvious (potentially more verbose) to express the same operation

"rules" / "naming conventions": prefixes to a noun/adj/verb or suffixes to a verb to modify it
* is-: function is a predicate
* un-: function has the opposite action
* -if: function takes a predicate
* -where
* -while
* -by: function takes a key-selector

categories of functions:
* predicate: returns a `Num` in 0 or 1, where 1 indicates the last argument verifies the tested property (eg. `isodd`, `isin {..}`...)
* key-selector: selects a specific part/property of its input for use in a function (typ. `len`, as in `sortby len`)
-->

# Library

(ie. libsel.so and such)

## Extending

The program can be extended by adding new names.

Adding a new name ("function" or "value") can be done in
two ways:
- as a builtin, directly in C++;
- in the prelude, expressed in sel.

The first one is the favored way as it give much more
liberty. The second one may be less performant, and many
computation are way more difficult to express in sel than
in C++.

### As a Builtin

"Builtin" referes to a sel value
which is implemented in C++, declared in
[builtins.hpp](lib/include/sel/builtins.hpp) and defined
in [builtins.cpp](lib/src/builtins.cpp). For it to be
accessible:
- declare it in the hpp using the appropriate macro
- register it in the hpp
- defind it in the cpp by implementing the needed method-s
- (optionally implement its codegen visitor method-s) -- NIY

#### Declaration

A set of C macro is used to declare new builtins. You
should use the one that has the name of the last type it
returns, for example:
- `BIN_num` for a function that ultimately returns a number,
  or a value that _is_ a number
- `BIN_str` for a function that ultimately returns a string,
  or a value that _is_ a string
- `BIN_lst` for a function that ultimately returns a list,
  or a value that _is_ a list (regardless of contains type)
- `BIN_unk` for a function which return type is parametric
  (eg. `[a] -> a`)

For any of these macro, the parameters are:
1. the name (as an identifier, must not contain `_` or digits)
2. the type (a comma-separated list of C++ MTP structures - careful of the lower case, that's my bad)
3. a helpful docstring
4. any member variables / methods it may need (this is the body of a structure)

> Note that the name defined **in C++** is affixed with
> `_`, this is to avoid collisions with eg. keywords.

Parameters for points 2 and 4 have to be specified between
parenthesises, as they could contain commas.

Here are a few declaration as examples:
```cpp
// map :: (a -> b) -> [a] -> [b]
BIN_lst(map, (fun<unk<'a'>, unk<'b'>>, lst<unk<'a'>>, lst<unk<'b'>>),
  "make a new list by applying an unary operation to each value from a list", ());

// tonum :: Str -> Num
BIN_num(tonum, (str, num),
  "convert a string into number", (
  // these 2 to cache the result
  double r;
  bool done = false;
));

// const :: a -> b -> a
BIN_unk(const, (unk<'a'>, unk<'b'>, unk<'a'>),
  "always evaluate to its first argument, ignoring its second argument", ());
```

A defined builtin will not be in
play until registered. Near the end of
[builtins.hpp](lib/include/sel/builtins.hpp) is a list
of all the registered builtin. Here you need to add your
own. It should be kept in alphabetical order. (Note again
that, **in C++**, `_` is affix to the name.)

#### Definition

Which methods must be implemented depends on the macro
that was used to declare it:
- `BIN_num`:
  - `double value()`
    return the actual numeric value
- `BIN_str`:
  - `std::ostream& stream(std::ostream&)`
    send a part of the string into the stream
  - `bool end() const`
    indicates whether it is valid to call `stream` again
  - `std::ostream& entire(std::ostream&)`
    send the entirety of the string into the stream
- `BIN_lst`:
  - `Val* operator*()`
    returns a pointer to the current value
  - `Lst& operator++()`
    fetches the next value; only exists in prefix notation;
    must returns `*this`
  - `bool end() const`
    indicates whether it is valid to use `*` or `++` again
- `BIN_unk`:
  - `Val* impl()`
    returns a pointer to the resulting value

In any member method, the `bind_args` macro can be used
to access the arguments as C++ references.
```cpp
Val* const_::impl() {
  // const :: a -> b -> a
  // const take ignore = take
  bind_args(take, ignore);
  return &take;
}
```

#### Codegen

(not implemented yet, but this would
be done by writing the needed method for
[VisCodegen](lib/src/visitors/codegen.cpp), which itself
will also requier adding the method declaration in
[the header](lib/include/sel/visitors.hpp)... none of
which exists yet)

### In the Prelude

(this mechanism is not clarified yet, but the prelude is
essentially a piece of sel script that gets prepended
to the user-input script; it contains calls to
[`def`](#Syntax))

---

> the GitHub "Need inspiration?" bit was "super-spoon"
