A versatile command line tool to deal with streams, with a
("point-free") functional approach.

---

## Overview

Similarly to commands such as `awk` or `sed`, `sel` takes
a script to apply to its standard input to produce its
standard output.

In its most basic form, the script given to `sel` is
a series of functions separated by `,` (comma). See the
complete syntax bellow. In this way, each function
transforms its input and passes its output to the next one:
```console
$ printf 12-42-27 | sel split :-:, map [add 1], join :-:, ln
13-43-28
$ printf abc | sel codepoints, map ln
97
98
99
```

## Syntax

Yes, this is the complete syntax (with help of some regex
for usual number/comment/identifier/bytestring):
```bnf
script ::= <apply> {',' <apply>}

apply ::= <value> {<value>}
value ::= <atom> | <subscr> | <list> | <pair>

atom ::= <word> | <bytes> | <number>
subscr ::= '[' <script> ']'
list ::= '{' [<apply> {',' <apply>} [',']] '}'
pair ::= <atom> '=' <atom>

word ::= /[a-z]+/ | '_'
bytes ::= /:([^:]|::)*:/
number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)/

comment ::= '#' /.*/ '\n'
```

The objective with it was to make it possible to type the
script plainly in any (most?) shell without worrying about
quoting much if at all:
- the script can span multiple arguments, they are joined
  naturally with a single space
- the single and double quotes are not used, so to feel
  safer the whole script can be quoted

The one case which can cause problem is lists (`{ .. }`)
which can be interpreted as glob _if they do not contain
a space_. For that reason, it is highly recommended to
keep the space after the `,` separating items.

## Types

Type notations are inspired from Haskell.
- number and bytestring: `Num` and `Str`
- list: `[a]`
- function: `a -> b`, when `b` is itself a function it
  will be `a -> x -> y`, but when `a` is a function then
  it is `(x -> y) -> b`
- pair: `(a, b)`

Lists and bytestring can take a `+` suffix (eg. `Str+`
and `[Num]+`) which represent a potentially unbounded
object (simplest example is `repeat 1 :: [Num]+` is an
infinite list of 1s).

The item type of a literal list is inferred as the list
is parsed:
- `{1, 2, 3, :soleil:}` not ok because inferred as `[Num]`
- `{repeat 1, {1}} :: [[Num]+]` ok because `{1}` can
  'lose' its bounded charateristic safely
- `{{1}, repeat 1}` not ok because inferred as `[[Num]]`
  at the first item and `repeat 1` can never 'lose' its
  unbounded charateristic safely

The CLI `-t` option will give the type of the whole expression.

## Builtins

~~Like every functional-ish languages~~ *ahem* it relies
on having a butt ton of existing functions. These can be
queried with `-l`:
```console
$ sel -l
[... list of every functions ...]
$ sel -l map add
map :: (a -> b) -> [a]+ -> [b]+
	make a new list by applying an unary operation to each value from a list
add :: Num -> Num -> Num
	add two numbers
$ sel -l :: 'a -> Num'
thread 'main' panicked at src/main.rs:103:43:
not yet implemented
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
```

## (wip and such)

### format better error report presentation

todo :)

### compile to linear sequence of instructions

- constant folding; because pure, identify what is not compile-time known:
  - can fold: literal (numbers, bytestrings), a list if all items can be folded, a call if all arguments are provided and can be folded
  - cannot fold: `input`, infinite sources cannot be turned into a finite structure but can still be expressed statically, 'control-point' functions and/or functions with side effects if ever
- [continuation-passing style (CPS)](https://en.wikipedia.org/wiki/Continuation-passing_style) (IR?)
- thunks? but I'm wondering if there is a way to even more directly put the instruction at the location at c-time rather than packing them at r-time
- lifetime tracking, or maybe 'duplication tracking'

- [linear type system](https://en.wikipedia.org/wiki/Substructural_type_system)
- [escape analysis](https://en.wikipedia.org/wiki/Escape_analysis)
- [random lecture](https://www.cs.cornell.edu/courses/cs4110/2018fa/lectures/lecture29.pdf)
- [random esopwheverthatmeans](https://www.cs.cornell.edu/people/fluet/research/substruct-regions/ESOP06/esop06.pdf)

### insert coersion functions automatically

 true type | wanted   | inserted         | behavior
-----------|----------|------------------|----------
 `Str+`    | `Num`    | `, tostr, `      | parse as a number, or `0`
 `Num`     | `Str`    | `, tonum, `      | writes the number in decimal
 `Str+`    | `[Str]+` | `, graphemes, `  | list of the Unicode graphemes
 `Str+`    | `[Num]+` | `, codepoints, ` | list of the Unicode codepoints
 `[Str+]+` | `Str+`   | `, join ::, `    | joined with empty separator

---

> the GitHub "Need inspiration?" bit was "super-spoon"
