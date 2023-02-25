# Values

TODO

## Str

`end` true indicates that `<<` op can not be performed,
ie. `end` indicates past-the-end.

strings differ from lists in that it advances automatically
when the `<<` op is used.

if the `<<` op was used even once, `entier` must not
be used; likewise `<<` op must not be used if `entier`
was called.

```cpp
while (!str.end()) {
  oss << str;
}


ostringstream oss;
string s = (str.entier(oss), oss.str());
```

## Lst

`end` true indicates that neither of `*` op and `++`
op can not be performed, ie. `end` indicates past-the-end.

```cpp
for (; !lst.end(); ++lst) {
  Val& it = *lst;
}
```
