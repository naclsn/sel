use std::ops::Index;

use crate::engine::{Apply, Array, Function, Number, Type, Typed, Value};

use dsl_macro_lib::val;

macro_rules! named_val {
    ($name:ident :: $($t:tt)->+ = $def:expr) => {
        ( stringify!($name).to_string()
        , stringify!($name :: $($t)->+).to_string() // .. and proper doc
        , val!($name :: $($t)->+ = $def)
        )
    };
}

#[rustfmt::skip]
fn _crap() -> Vec<(String, String, Value)> {
    vec![

        named_val!(add :: Num -> Num -> Num = |a: Number, b: Number| a + b),
        named_val!(sum :: [Num] -> Num = |a: Vec<Number>| a.iter().fold(0.0, |a, b| a + b)),

        named_val!(id :: a -> a = |a: Value| a),

        named_val!(map :: (a -> b) -> [a] -> [b] = |f: Function, a: Vec<Value>|
            a   .into_iter()
                .map(|i| f.clone().apply(i))
                .collect::<Value>()
        ),

        named_val!(find :: ([a] -> Num) -> [a] -> a = |_f: Function, _a: Vec<Value>| "coucou"),

        named_val!(tonum :: Str -> Num = |s: String| s.parse().unwrap_or(0.0)),
        named_val!(tostr :: Num -> Str = |n: Number| n.to_string()),

        named_val!(pi :: Num = 3.14),
        named_val!(idk :: [Num] = [1.0, 1.0, 2.0, 3.0, 5.0, 8.0]),
    ]
}
