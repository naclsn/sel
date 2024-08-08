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

Complete syntax:
```bnf
script ::= <apply> {',' <apply>}

apply ::= <value> {<value>}
value ::= <atom> | <subscr> | <list>

atom ::= <word> | <bytes> | <number>
subscr ::= '[' <script> ']'
list ::= '{' [<apply> {',' <apply>} [',']] '}'

word ::= /[a-z]+/
bytes ::= /:([^:]|::)*:/
number ::= /0b[01]+/ | /0o[0-7]+/ | /0x[0-9A-Fa-f]+/ | /[0-9]+(\.[0-9]+)/

comment ::= '#' /.*/ '\n'
```

<!--Help on individual functions can be obtained with `-l`:
```console
$ sel -l map add
map :: (a -> b) -> [a] -> [b]
	make a new list by applying an unary operation to each value from a list

add :: Num -> Num -> Num
	add two numbers

$ sel -l
[... list of every functions ...]
```-->

## Getting Started

cargo.

## Setting Up for Development

cargo.

---

> the GitHub "Need inspiration?" bit was "super-spoon"
