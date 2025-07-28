A versatile command line tool to deal with streams, with a
(mostly point-free) functional approach.

<!--

cargo install insta
cargo install grcov
rustup component add llvm-tools



CARGO_INCREMENTAL=0 RUSTFLAGS=-Cinstrument-coverage cargo test
grcov . -s . --binary-path ./target/debug/ -t html --branch --ignore-not-existing -o ./target/debug/coverage/
xo target/debug/coverage/index.html

grcov . -s . --binary-path ./target/debug/deps/ -t html --branch --ignore-not-existing -o ./target/debug/coverage/





grcov --source-dir src/ --binary-path target/debug/deps/ --output-types html --branch --ignore-not-existing --output-path target/debug/coverage/ default_7193634055680149699_0_16794.profraw

-->

---

#### State of Development

State: ~~working and usable _but_ I want to 'compile', that
is, down to a linear sequence of instructions. Current way
of interpreting is very wasteful.~~ broken every other day!

Also see the 'wip and such' below.

---

## Overview

Similarly to commands such as `awk` or `sed`, `sel` takes
a script to apply to its standard input to produce its
standard output.

In its most basic form, the script given to `sel` is
a series of functions separated by `,` (comma). See the
complete syntax bellow. In this way, each function
transforms its input and passes its output to the next one
(`-` is the function that returns the input stream):
```console
$ printf 12-42-27 | sel -, split :-:, map [add 1], join :-:
13-43-28
$ printf abc | sel -, codepoints # same as 'sel codepoint -'
97
98
99
```

When the first argument names a file starting with `#!`
the file is read and parsed first. Any additional arguments
are also parsed in continuation of the script.
```console
$ cat pred.sel
#!/usr/bin/env sel
sub 1
$ sel pred.sel 5
4
```

## Syntax

TLDR:
- lists: `{0b1, 0o2, 0x3, 4.2}`, strings: `:hi how you:`
- `my-func first-arg [add 1 2] third-arg`
- `f, g` is `g(f(..))` (or `pipe f g` or `[flip compose] g f`)

Special characters and keywords:
`,` `:` `=` `[` `]` `def` `let` `use` `{` `}`

3 special forms:
- `def name :description: value` will define a new name
  that essentially replaces by value where it is used;
- `use :some/file: f` will 'import' all the defined names
  from the file as `f-<name>` (or as is if using `_`);
- `let pattern result fallback` will make a function of one
  argument that computes result if pattern matches, pattern
  can introduces names (eg `let {a, b,, rest} [add a b] 0`,
  the `,, rest` matches the rest of the list), if pattern
  is irrefutable then there is no fallback.

Here is the complete syntax:
```bnf
top ::= {'use' <bytes> <word> ','} {'def' <word> <bytes> <value> ','} [<script>]
script ::= <apply> {',' <apply>}

apply ::= (<binding> | <value>) {<value>}
value ::= <atom> | <subscr> | <list> | <pair>

binding ::= 'let' (<irrefut> <value> | <pattern> <value> <value>)
irrefut ::= <word> | <irrefut> '=' <irrefut>
pattern ::= <atom> | <patlist> | <patpair>
patlist ::= '{' [<pattern> {',' <pattern>} [',' [',' <word>]]] '}'
patpair ::= (<atom> | <patlist>) '=' <pattern>

atom ::= <word> | <bytes> | <number>
subscr ::= '[' <script> ']'
list ::= '{' [<apply> {',' <apply>} [',' [',' <apply>]]] '}'
pair ::= (<atom> | <subscr> | <list>) '=' <value>

word ::= /[-a-z]+/ | '_'
bytes ::= /:([^:]|::)*:/
number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)?/

comment ::= '#-' <balanced> [','] | '#' /.*/ '\n'
balanced ::= '[' <matched> ']' | '{' <matched> '}' | <bytes> | /[^\t\n\f\r ]+/
```
<!-- should dash comments eat the ','? seems practical but idk -->

The objective here was to make it possible to type the
script plainly in any (most?) shell without worrying about
quoting much if at all:
- the script can span multiple arguments, they are joined
  naturally with a single space
- the single and double quotes are not used, so to feel
  safer the whole script can be quoted

One case which can cause problem is lists (`{ .. }`) which
can be interpreted as glob _if not containing a space_.
For that reason, it is highly recommended to keep the space
after the `,` separating items.

## Types

Type notations are inspired by Haskell:
- number and bytestring: `Num` and `Str`;
- list: `[a]`;
- function: `a -> b`, when `b` is itself a function it
  will be `a -> x -> y`, but when `a` is a function then
  it is `(x -> y) -> b`;
- pair: `(a, b)`.

Lists and bytestring can take a `+` suffix (eg. `Str+`
and `[Num]+`) which represent a potentially unbounded
value (simplest example is `repeat 1 :: [Num]+`, an
infinite list of 1s).

The item type of a literal list is inferred as the list
is parsed:
- `{1, 2, 3, :soleil:}` not ok because inferred as `[Num]`
- `{repeat 1, {1}} :: [[Num]+]` ok because `{1}` can
  'lose' its bounded charateristic safely
- `{{1}, repeat 1}` not ok because inferred as `[[Num]]`
  at the first item and `repeat 1` can never 'lose' its
  unbounded charateristic safely
TODO: idk if this still the case

The CLI `-t` option will give the type of the expression.

## "Auto Coercion"

When a direct function argument doesn't match the parameter,
one of these function is automatically inserted:

 wanted   | true type | inserted
----------|-----------|--------------------
 `Num`    | `Str+`    | `, tonum, `
 `Str`    | `Num`     | `, tostr, `
 `[Num]+` | `Str+`    | `, codepoints, `
 `[Str]+` | `Str+`    | `, graphemes, `
 `Str+`   | `[Str+]+` | `, ungraphemes, `
 `Str+`   | `[Num]+`  | `, uncodepoints, `

There is also a for now temporary behavior on the output
depending on the type:
- `Num`: printed with a newline
- `(a, b)`: printed with a tabulation between the two and a newline
- `Str`: printed as is
- `[a]`: printed with a newline after each entries

## Builtins and Prelude

The existing functions can be queried with `-l`:
```console
$ sel -l
[... list of every functions ...]
$ sel -l map add
map :: (a -> b) -> [a]+ -> [b]+
	make a new list by applying an unary operation to each value from a list
add :: Num -> Num -> Num
	add two numbers
$ sel -l :: 'a -> Num'
[... list of matching functions ...]
```

<!-- There is also an undocumented word that completely aborts the parsing: `fatal`. -->

## Ack & Unrelated

Python, <!-- for the frustration and motivation it provides -->
Haskell, <!-- because . point . free . funny -->
Rust, <!-- # blazingly fast -->
[KDL](https://kdl.dev/), <!-- the "slashdash" comments! -->
[jq](https://github.com/jqlang/jq), <!-- similar concept, great but unusable unintuitive imo -->
[tree-sitter](https://github.com/tree-sitter/tree-sitter), <!-- idk, initial attempt used that -->
[dt](https://github.com/so-dang-cool/dt), <!-- found way after `sel` was concretized, but that exactly the same thing! -->
[Helix](https://github.com/helix-editor/helix) <!-- back then, made me need a script I can easily write unquoted and filter each cursor selection through.. and python isnt it, it would only make it painful -->

---

## (wip and such)

### `types`

- try to free indices that are not used
- polish for cases such as 2 `a`s being distinct
- ex of inf type `(a -> a) -> a  <-  (b -> Num) -> b`
- something about pseudo syntaxes in named type ('paramof', 'returnof', 'a=b', also '?' and '?abc')

### `def`s

something like `$PYTHONSTARTUP`, between prelude and user script

process description of `def`s (eg. markdown-ish?)
maybe name for var types in there

### regarding coercion:

- `{1, 2, 3}, map ln` could tostr in map
- `split :-:, map [add1]` could tonum in map
- `add 1, tonum` could tostr in between

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

---

> the GitHub "Need inspiration?" bit was "super-spoon"
