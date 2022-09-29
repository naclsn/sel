Some thing are (already) no longer up to date, see end of
'Assembling Pipe Fittings'.

Yet an other `awk`-ish command line tool, with a
(point-free\*) functional approach (and written in Rust
or whatever -- if I don't give up on it).

> \* Not sure this will stay forever, but it is being
> designed to be and is easier to use as such (in fact
> there is no syntax for lambda notations).

## Overview

Similarly to commands such as `awk`, `sed` or `perl
-pe`/`perl -ne`, `sel` takes a script to apply to its
standard input to produce its standard output.

A script is like a pipeline. Each pipe fitting (or fit)
is a function of 1 input to 1 output. Here is a simple
example (try to guess its behavior):
```console
$ sel TODO: LOL, FIND AN EXAMPLE HERE ><
```

Each function of the pipeline is given as its own word
(in the shell sens). Most of the time, each argument is a
fit. Note that `sel` scripts barely ever need to be quoted.
<!-- ok, what even was that suppose to say? rewrite,
reformulate or do something; this is more confusing than
anything else -->

## Getting Sfarted

idk, probly smthing like:
```console
$ cargo build --release
...
$ seq 17 42 | target/release/sel if all ] id] # isprime, if I can, for fun :3
17
19
23
29
31
37
41
```

---

## Unsorted Details

### Type System

The types that can flow through a fit are:
 - string (`str`)
 - number (`num`)
 - array of string or number (`@str` or `@num`)

Note the following for implicit coercion:
 actual type | destination | behavior
-------------|-------------|----------
 str         | num         | [see number parsing]()
 str         | @num        | array of the Unicode [codepoints]()
 str         | @str        | array of the Unicode [characters]()
 num         | str         | [see number printing]()
<!-- 'characters': or full-on graphems? -->
<!-- 'printing': probly just base 10, idk -->

Type coersion is a bad thing and should only be used
carefuly and when you know exactly what you are doing
(ie. most of the time).  Here it is mainly useful because
the first fit of the line gets for input the current line,
which is a string.

To make documentation and references easier to understand,
the following types may be used:
 - `bool` usually when describing a condition
 - `fit` when the input and output types of a fit are not relevant (otherwise uses the arrow notation)
 - `any` when any type is applicable or when the type is not relevant

Values considered false in conditions (defining the `bool`
type) are the following (anything else is considered true):
 - the empty string
 - the empty array
 - the number 0

### Builtins and Operators

A set of builtin fits are provided (see the [list
somewhere]()). Most of these are spelled-out in lower-case
english and follow regular [naming rules](). With these,
a set of non-alpha-numeric characters have been carefully
picked to be safe to use unquoted in a POSIX-ish shell.

These are binary operators used with infix syntax:
 token | type                 | meaning
-------|----------------------|---------
 `+`   | `num -> num -> num`  | addition
 `-`   | `num -> num -> num`  | substraction
 `.`   | `num -> num -> num`  | multiplication
 `/`   | `num -> num -> num`  | division
 `%`\* | `num -> num -> num`  | mod or rem
 `:`   | `num -> num -> @num` | range constructor (see [ranges]())
 `#`   | `fit -> fit -> fit`  | application operator (similar to Haskell's `($)`, see [when use it]())
 ` `   | `fit -> fit -> fit`  | composition operator (similar to Haskell's `flip (.)`)
 `^`\* | `str -> str -> @str` | string splitting
 `,`\* | `@str -> str -> str` | array joining (first arg could change to `@any`...)
 `_`\* | `@any -> @any -> @any` | array indexing, also exist with a scalar first argument (fwaa, but this means overload? or is it coersion from scalar to array? both are not great.. but having to write eg. `_{0}` is not great either...)
 `=`\* | -> bool | when comparing `num` and `str`, tries converting the `str` to `num` first; arrays are compared by comparing each element

> \* Still being discussed.

These are unary operators used with prefix syntax:
 token | type            | meaning
-------|-----------------|---------
 `@`   | `str -> @str`\* | array constructor (see [about arrays something]())
 `~`\* | `fit -> fit`    | flip the 2 first arguments of fit (but no because ~/) if not the probably a bin op for 'not equal'
 `-`\* | `num -> num`    | negation (but may remove)

> \* Still being discussed.

Of course unary and binary operators can still be used when
a function (of matching type) is expected. For example in
`fold+` (equivalent to `sum`) or `map-`.

### Assembling Pipe Fittings

Many fits work by taking as parameter one or more other
fits, for example the following script only contains 1
(top-level) fit:
```console
$ seq 11 | sel if#iseven#sqrt
```

Here is the type definition of `if`:
`if :: (any -> bool) -> fit -> fit`

Its first parameter is the condition, and its second on
is the consequence. The condition should be a fit which
output value will be evaluated as a condition to execute
or not the consequence.

Because each fit of the script needs to be a single word,
the `#` operator needs to be used. To see the difference,
here is a JS-ish re-writting (say `$if` is the `if`
function):
```js
function $if(condition) {
  return function (consequence) {
    return function (input) {
      if (condition(input))
        return consequence(output);
    }
  }
}
function iseven(n) {
  return 0 == n%2;
}
function sqrt(n) {
  return Math.sqrt(n);
}

// with:  if#iseven#sqrt
output = $if(iseven)(sqrt)(input);

// with:  if iseven sqrt
output = sqrt(iseven($if(input)));
```

Note that the `#` operator is only explicitely needed
when the parameter would merge with the function. That
is to construction a fit which increments its input,
`+1` can simply be written.

With this, it is possible to make each fit a single word.
There is however a special case in which a single fit may
span over multiple word:
```console
$ printenv PATH | tr : \\n | sel readdir filter basename len =2]
```

In the above invocation, `sel` receives a path per line and
for each line: reads the directory and filter the entries
which basename is of length 2 (the command will essentially
scan the PATH for every 2-letter executables).

The type definition of `filter` is as follow:
`filter :: (any -> bool) -> fit`

In the example, its parameter (the filtering predicate)
is the whole `basename len =2`. Because this parameter
needs to contain spaces, the `]` syntax is used: it marks
the end of the parameter to filter.

The same pipeline can be written this way (which is not
really shorter nor more readable):
```console
$ printenv PATH | sel split{:} map readdir filter basename len =2]] join{\\n}
```

Syntax update:
```console
$ printenv PATH | sel split{:}, map [readdir, filter [basename, len =2]] join{\\n}
```
`[` has been assumed safe enough: as long as the content
contains a space, glob expansion cannot interfere.

### Literal Strings and Arrays

<!--
The syntax is somewhere around `a, b, c` where each can
be an item or a range; so for example:
 in `{}`                | yields
------------------------|--------
 `1, 2, 3`              | json [1, 2, 3]
 `1, 5:9`               | json [1, 5, 6, 7, 8]
 `1, 9:5`               | json [1, 9, 8, 7, 6] -- idk about end-point
 `this, is, some, text` | json ["this", "is", "some", "text"]
 `text, 42, more`       | json ["text", 42, "more"]

When the text cannot be parsed as a number, it is a
string. In this case spaces at its begining and end are
trimmed. Similarly, a string containing a `:` is only
converted to a range if both ends are valid numbers. (But
then this is just the same as keeping every element not
range a string, right? because string a converted to
numbers whenever needed?)

Also, on the parse level, a `{}` literal can contain a `}`
and be valid if it had a matching `{` before it:
 input source         | note
----------------------|------
 {hello {some} world} | valid
 {bla } bla}          | invalid (or rather, the string stops on the first `}`
 {bla { bla}          | invalid (because it is looking for a closing `}`)

Having matching paris `{}` in literals allow for nested array definition
-->

### Builtin Naming Rules

> idk, just hope and next thing you know the whole api is _noise_

### (probably) Complete Grammar

No longer up to date, see end of 'Assembling Pipe Fittings'.

```bnf
script ::= {fitting}
fitting ::=
  | expression
  | partial

expression ::=
  | identifier
  | literal
  | first binop second
  | unop only
  | base_simple
  | base complex_']'

(* the _ here just means 'immediate', ie no space *)
inline note that if the ',' is used instead of '#', it (I)
/maybe/ would be ok to remove the `base_simple` syntax...

partial ::=
  | first binop
  | binop second

literal ::=
  | number
  | string

(* fist, second, only, base, simple, complex ::= expression *)

identifier ::= /[a-z]+/
number     ::= /[0-9]+(\.[0-9]+)?|0x[0-9A-F]+|0b[01]+|0o[0-7]/
string     ::= /\{([^}](?R))\}/
binop      ::= /[#%+,\-./:=^_~]/  (* TBD / this will change *)
unop       ::= /[-@]/             (* TBD / this will change *)

```
<!--
Few thing to be noted about the grammar.

The '-' serves both as unary and as a binary. This means
that only the context makes in possible to distinguish
`-1` as 'negative one' or 'substract one'. 'negative one'
should only be expected where a scalar can be. (ie.? this
enough? see again with the example of `fold+` and `map-`)
-- k, this is very likely to be removed (ie. no unary);
   problem is '_' is already used for indexing (so maybe
   if something else takes this role, 'could be eg.
   `idx`) so it won't be use for here.. so I guess the
   question now is how much is the unary minus needed?

The '.' serves both as a binary and as the decimal
separator. Because literal number needs to be spelled
out fully (ie. no eg. `.5`) this makes distinguishing
between 'a half' and 'times five' easy. The problem is
when a scalar is expected. In this case it should always
be assumed to be the literal, not the product (so `5.2`
is not the value ten).

How does nouns and verbs bind? For example in `map1/`.
Does it work? (yes, straight up asking, I don't know)
 - if it is allowed, probably better to always have
   everything bind from the end (or smth like that)
 - if not allowed this means that the '#' is needed
   as in `map#1/` (if not '#' then other)
-->

<!--
other notes

somewhat safe in sh (and ba- z- da- ..)
%+,-./:@]^_{}

used (adds '=', '#' and '~', this last one needs a better that flip 'cause ~/ won't be good)
#%+,-./:=@]^_{}~

almost always unsafe (rem: '!' is history event, can only be followed by space, '=' or '(')
!*?[

still somewhat undecided
#%,=^~

could use the ',' where the '#' is currently:
 - pro no shift (at least or us and fr lul)
 - con: is it maybe not quite natural?

https://code.jsoftware.com/wiki/NuVoc
https://code.jsoftware.com/wiki/Vocabulary/PartsOfSpeech#Modifier
https://esolangs.org/wiki/%E2%86%90THE_LAST_ACTION_LANGUAGE%E2%86%92
https://www2.cs.arizona.edu/icon/
https://github.com/stedolan/jq
https://github.com/yamafaktory/jql
-->

<!-- on can dream -->
