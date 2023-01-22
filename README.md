A versatile command line tool to deal with streams, with a
("point-free") functional approach.

---

#### State of Development

The project has be brought to a "usable" (~hackable) point.
However it is unfinished, notably:
- tests are coming slowly
- infrastructure for tuples and pattern lists is in place, but incomplete
- same goes for affixing types with `*` (marks unbounded)
- (.. because of the previous one, literal lists are broken and crash type-checking)
- not every function that I would want are implemented, plan is to add as the need comes
- LLVM-based codegen is still far

---

## Overview

Similarly to commands such as `awk` or `sed`, `sel` takes
a script to apply to its standard input to produce its
standard output.

In its most basic form, the script given to `sel` is
a serie of functions separated by `,` (comma). See the
[complete syntax](doc/Scripting/Syntax.md). Each function
transforms its input and passes its output to the next one:
```console
$ printf 12-42-27 | sel split :-:, map [add 1], join :-:, ln
13-43-28
$ printf abc | sel codepoints, map ln
97
98
99
```

Help on individual functions can be obtained with `-l`:
```console
$ sel -l map add
map :: (a -> b) -> [a] -> [b]
	make a new list by applying an unary operation to each value from a list

add :: Num -> Num -> Num
	add two numbers

$ sel -l
[... list of every functions ...]
```

## Getting Started

Project uses the meson build system. With ninja:
```console
$ meson setup build/
$ ninja -C build/
...
$ build/sel -h
```

> Temporarly, more on command usage can be found [here](doc/README.md#Command%20Usage).

From there, this will install the executable `sel` and
the library `libsel.so`, as well as shell completion
(if it exists -- implemented: bash):
```console
$ ninja -C build/ install
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

## Setting Up for Development

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

The folder [s/](s) contains the following shell scripts:
- `s/el`: build and exec as the resulting binary
- `s/el-db`: same idea, but use `gdb` (and thus cannot read from stdin)
- `s/test`: run the tests given, or all the tests by default
- `s/cover`: generate coverage report (also `xdg-open`s it)

---

> the GitHub "Need inspiration?" bit was "super-spoon"
