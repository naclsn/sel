- [Scripting](Scripting)
  - [Syntax](Scripting/Syntax.md)
  - [Type-System](Scripting/Type-System.md)
  - [Operators](Scripting/Operators.md)
  - [Literals](Scripting/Literals.md)
  - [Builtins](Scripting/Builtins.md)
- [Library](Library)
  - [Values](Library/Values.md)
- [Development](Development)
  - [Extending](Development/Extending.md)
  - [Tests](Development/Tests.md)

---

## Command Usage

> The actual "front-end" is not fully designed yet.

```console
$ build/sel -h
Usage: build/sel [-Dns] <script...> | -f <file>
       build/sel -l [<names...>] | :: <type...>
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
- `-D` does not execute the script but prints dev information (format unspecified).

The second form provides lookup and help for specified
names. When no names are given, every known names are
listed out (one by line). It can also be used to find a
value matching a type: `sel -l :: 'Num -> Str'`.

With the last form, it is possible to make `sel` generate
a small executable which performs the provided script. It
is compiled leveraging LLVM, so it has to be available on
the system. This is not implemented yet.
