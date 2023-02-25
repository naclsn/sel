# Syntax

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

---

Complete grammar:
```ebnf
<comment> ::= '#' (* anything *) '\n'

<script> ::= <element> {(',' | ';') <element>}
<element> ::= <atom> {<atom>}

<atom> ::= <literal>
         | <name>
         | <subscript>
         | <unop> [<binop>] <atom>
         | <binop> <atom>

<literal> ::= <number> | <string> | <list>
<name> ::= /[a-z]+/
<subscript> ::= '[' <script> ']'
<unop> ::= '%' | '@'
<binop> ::= '+' | '-' | '.' | '/' | '=' | '_'

<number> ::= /[0-9]+(\.[0-9]+)?/ | /0x[0-9A-F]+/ | /0b[01]+/ | /0o[0-7]/
<string> ::= ':' (* anything, ':' have to be doubled *) ':'
<list> ::= '{' <element> {',' <element>} '}'

(* characters '~' and '^' are reserved a potential operators *)
```
