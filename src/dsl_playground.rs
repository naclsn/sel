use std::ops::Index;

use crate::engine::{Apply, Array, Function, Type, Typed, Value, Number};

use dsl_macro_lib::val;

macro_rules! named_val {
    ($name:ident :: $($t:tt)->+ = $def:expr) => {
        ( stringify!($name).to_string()
        , stringify!($name :: $($t)->+).to_string() // .. and proper doc
        , val!($name :: $($t)->+ = $def)
        )
    };
}

fn _crap() -> Vec<(String, String, Value)> {
    vec![

    named_val!(add :: Num -> Num -> Num = |a: Number, b: Number| a + b),
    named_val!(sum :: [Num] -> Num = |a: Vec<Number>| a.iter().fold(0.0, |a, b| a + b)),

    named_val!(id :: a -> a = |a: Value| a),

    // named_val!(map :: (a -> b) -> [a] -> [b] = |f: Function, a: Array|
    //     a.items
    //         .into_iter()
    //         .map(|i| f.clone().apply(i))
    //         .collect()
    // ),

    named_val!(tonum :: Str -> Num = |s: String| s.parse().unwrap_or(0.0)),
    named_val!(tostr :: Num -> Str = |n: Number| n.to_string()),

    named_val!(pi :: Num = 3.14),
    named_val!(idk :: [Num] = [1.0, 1.0, 2.0, 3.0, 5.0, 8.0]),
    named_val!(idk :: [Num] = Value::Arr(Array { has: Type::Num, items: vec![1.0.into(), 1.0.into(), 2.0.into(), 3.0.into(), 5.0.into(), 8.0.into()] })),

    ]
}
