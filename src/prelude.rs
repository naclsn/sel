use crate::engine::{Fun, Type, Value};

macro_rules! count {
    ($_:expr,) => { 1usize };
    ($h:expr, $($t:expr,)*) => { 1usize + count!($($t,)*) };
}

macro_rules! ty_for {
    ($simple:ident) => { Type::$simple }
}

macro_rules! val_for {
    ($simple:ident) => { Value::$simple }
}

macro_rules! match_for {
    ($simple:ident, $v:expr) => {
        match $v {
            Value::$simple(v) => v,
            other => panic!("match_ex_val: type mismatch (expected a {:?} got {other:?}", Type::$simple),
        }
    }
}

macro_rules! fun {
    ($name:ident :: [$($ty:ident),*] => $re:ident, $fn:expr) => {
        Fun {
            name: stringify!($name),
            params: &[$(ty_for!($ty)),*],
            ret: ty_for!($re),
            func: |args| {
                assert!(count!($($ty,)*) == args.len());
                let mut k: usize = 0;
                val_for!($re)($fn($(
                    match_for!($ty, &args[{ k+= 1; k-1 }])
                    // match &args[{ k+= 1; k-1 }] {
                    //     Value::$ty(v) => v,
                    //     other => panic!("Type mismatch: expected a {:?} got {other:?}", Type::$ty),
                    // }
                ),*))
            },
            args: vec![], // alloc..?
        }
    };
} // fun!

// TODO: have doc inline
// FIXME: cannot use in const or static (because of allocations)
const LU: &'static [Fun] = &[
    fun!(add :: [Num, Num] => Num, |a, b| a + b),
    // fun!(crap :: [] => Num, || 0), // FIXME: cannot do 0 arity
    fun!(reverse :: [Str] => Str, |a: &String| a.chars().rev().collect()), // FIXME: cannot use nested types (ie. Arr<_>)
    // fun!(map :: [Fun<a,b>, Arr<a>] => Arr<b>, |a| todo!()),
];

pub fn lookup_prelude<'a>(name: String) -> Option<Fun> {
    LU
        .binary_search_by_key(&name.as_str(), |it| it.name)
        .map(|k| LU[k].clone())
        .ok()
}
