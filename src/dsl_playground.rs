use std::ops::Index;

use crate::engine::{Apply, Array, Function, Type, Typed, Value, Number};

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

impl From<Vec<f32>> for Value {
    fn from(_: Vec<f32>) -> Self {
        todo!()
    }
}
impl From<Value> for Vec<f32> {
    fn from(v: Value) -> Self {
        match v {
            Value::Arr(a) => a.items.into_iter().map(|it| it.into()).collect(),
            _ => unreachable!(),
        }
    }
}

fn _crap() -> Vec<(String, String, Value)> {
    let r: Vec<f32> = vec![1.0, 1.0, 2.0, 3.0, 5.0, 8.0];
    let v: Value = Value::from(r);
    let w: f32 = v.into();
    let z: Value = Value::from(Value::Num(42.0));

    vec![

    named_val!(add :: Num -> Num -> Num = |a, b| a + b),
    named_val!(sum :: [Num] -> Num = |a: Vec<Number>| {
        a.items
            .iter()
            .fold(0.0, |acc, cur| acc + cur)
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
    named_val!(idk :: [Num] = [1, 1, 2, 3, 5, 8]),

    ]
}
