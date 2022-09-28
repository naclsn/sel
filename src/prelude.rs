use crate::engine::{Fit, Value};

// YYY: make macro(s)
//      have doc inline
const LU: [Fit; 3] = [
    Fit { name: "add", arity: 2, func: |args| {
        assert!(2 == args.len(), "wrong number of argument");
        match (&args[0], &args[1]) {
            (Value::Num(a), Value::Num(b)) => Value::Num(a+b),
            _ => panic!("wrong argument type"),
        }
    } },
    // YYY: have arity 0? pro: all in the prelude, no
    // difference (from the outside) between functions and
    // values; con: uh.. will have to match for Fit where
    // a value is expected..?
    Fit { name: "crap", arity: 0, func: |_| Value::Num(0) },
    Fit { name: "reverse", arity: 1, func: |args| {
        assert!(1 == args.len(), "wrong number of argument");
        match &args[0] {
            Value::Str(c) => Value::Str(c.chars().rev().collect()),
            _ => panic!("wrong argument type"),
        }
    } },
];

pub fn lookup_prelude<'a>(name: String) -> Option<Fit> {
    LU
        .binary_search_by_key(&name.as_str(), |it| it.name)
        .map(|k| LU[k].clone())
        .ok()
}

