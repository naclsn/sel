use crate::engine::{Fun, Type, Value};

macro_rules! count {
    ($_:expr,) => { 1usize };
    ($h:expr, $($t:expr,)*) => { 1usize + count!($($t,)*) };
}

// idk why, everything started crapping the bed :-(
macro_rules! fun {
    ($name:ident :: $($ty:ident)->* => $re:ident, $fn:expr) => {
        Fun {
            name: stringify!($name),
            params: vec![$(Type::$ty),*], // alloc
            func: |args| {
                assert!(count!($($ty,)*) == args.len());
                let mut k: usize = 0;
                Value::$re($fn($(
                    match args[{ k+= 1; k-1 }] {
                        Value::$ty(v) => v,
                        other => panic!("Type mismatch (argument {k}): expected a {:?} got {other:?}", Type::$ty),
                    }
                ),*))
            },
            args: vec![], // alloc
        }
    };
} // fun!

const ADD: Fun = fun!(add :: Num -> Num => Num, |a, b| a + b);

// TODO: have doc inline
// FIXME: cannot use in const or static (because of allocations)
const LU: [Fun; 2] = [
    fun!(add :: Num -> Num => Num, |a, b| a + b),
    // fun!(crap :: => Num, || 0), // FIXME: cannot do 0 arity
    fun!(reverse :: Str => Str, |a: String| a.chars().rev().collect()), // FIXME: cannot use nested types (ie. Arr<_>)
];

pub fn lookup_prelude<'a>(name: String) -> Option<Fun> {
    LU
        .binary_search_by_key(&name.as_str(), |it| it.name)
        .map(|k| LU[k].clone())
        .ok()
}
