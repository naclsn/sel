use std::ops::Index;

use crate::engine::{Apply, Array, Function, Type, Typed, Value};

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

    named_val!(add :: Num -> Num -> Num = |a, b| a + b),
    named_val!(sum :: [Num] -> Num = |a: Array| 0.0),

    // named_val!(id :: a -> a = |a| a),

    // named_val!(map :: (a -> b) -> [a] -> [b] = |f: Function, a: Array|
    //     a.items
    //         .into_iter()
    //         .map(|i| f.clone().apply(i))
    //         .collect()
    // ),

    // named_val!(tonum :: Str -> Num = |s| s.parse().unwrap_or(0.0)),
    // named_val!(tostr :: Num -> Str = |n| n.to_string()),

    named_val!(pi :: Num = 3.14),
    named_val!(idk :: [Num] = vec![Value::Num(1.0), Value::Num(1.0), Value::Num(2.0), Value::Num(3.0), Value::Num(5.0), Value::Num(8.0)]),

    ]
}
