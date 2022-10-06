use std::ops::Index;

use crate::engine::{Apply, Array, Function, Type, Typed, Value};

use dsl_macro_lib::val;

macro_rules! sure {
    (let $id:ident = ($ty:ident)$vl:expr; $do:expr) => {
        match $vl {
            Value::$ty($id) => $do,
            _ => unreachable!(),
        }
    };
    (($ty:ident)$vl:ident) => {
        match $vl {
            Value::$ty(v) => v,
            _ => unreachable!(),
        }
    };
}

macro_rules! named_val {
    ($name:ident :: $($t:tt)->+ = $def:expr) => {
        ( stringify!($name).to_string()
        , stringify!($name :: $($t)->+).to_string() // .. and proper doc
        , val!($name :: $($t)->+ = $def)
        )
    };
}

impl From<f32> for Value {
    fn from(_: f32) -> Self {
        todo!()
    }
}

impl From<Value> for f32 {
    fn from(_: Value) -> Self {
        todo!()
    }
}

fn _crap() -> Vec<(String, String, Value)> {
    let r: f32 = 0.0;
    let v: Value = Value::from(r);
    let w: f32 = v.into();

    vec![

    named_val!(add :: Num -> Num -> Num = |a, b| a + b),
    named_val!(sum :: [Num] -> Num = |a: Array| {
        a.items
            .iter()
            .fold(0.0, |acc, cur| acc + sure!((Num)cur))
    }),

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
