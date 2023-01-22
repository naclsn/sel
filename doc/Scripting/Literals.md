# Literals

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
