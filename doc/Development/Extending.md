# Extending

The program can be extended by adding new names.

Adding a new name ("function" or "value") can be done in
two ways:
- as a builtin, directly in C++;
- in the prelude, expressed in sel.

The first one is the favored way as it give much more
liberty. The second one may be less performant, and many
computation are way more difficult to express in sel than
in C++.

## As a Builtin

"Builtin" referes to a sel value
which is implemented in C++, declared in
[builtins.hpp](lib/include/sel/builtins.hpp) and defined
in [builtins.cpp](lib/src/builtins.cpp). For it to be
accessible:
- declare it in the hpp using the appropriate macro
- register it in the hpp
- defind it in the cpp by implementing the needed method-s
- (optionally implement its codegen visitor method-s) -- NIY

### Declaration

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

### Definition

Which methods must be implemented depends on the macro
that was used to declare it:
- `BIN_num`:
  - `double value()`
    return the actual numeric value
- `BIN_str`:
  - `std::ostream& stream(std::ostream&)`
    send a part of the string into the stream
  - `bool end()`
    indicates whether it is valid to call `stream` again
  - `std::ostream& entire(std::ostream&)`
    send the entirety of the string into the stream
- `BIN_lst`:
  - `Val* operator*()`
    returns a pointer to the current value
  - `Lst& operator++()`
    fetches the next value; only exists in prefix notation;
    must returns `*this`
  - `bool end()`
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

### Codegen

(not implemented yet, but this would
be done by writing the needed method for
[VisCodegen](lib/src/visitors/codegen.cpp), which itself
will also requier adding the method declaration in
[the header](lib/include/sel/visitors.hpp)... none of
which exists yet)

## In the Prelude

(this mechanism is not clarified yet, but the prelude is
essentially a piece of sel script that gets prepended
to the user-input script; it contains calls to
[`def`](#Syntax))

---

> the GitHub "Need inspiration?" bit was "super-spoon"
