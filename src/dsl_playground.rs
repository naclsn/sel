use std::ops::Index;

use crate::engine::{Apply, Array, Function, Type, Typed, Value};

// TODO: eventually, I would like to move every
//       macro into this lib crate
use dsl_macro_lib::val;

// /// Goes from DSL type representation to actual Type instance.
// macro_rules! ty_ty {
//     (($t:tt)) => { ty_ty!($t) };
//     (($a:tt -> $($b:tt)->+)) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($($b)->+))) };
//     (Num) => { Type::Num };
//     (Str) => { Type::Str };
//     ([$t:tt]) => { Type::Arr(Box::new(ty_ty!($t))) };
//     ($a:tt -> $($b:tt)->+) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($($b)->+))) };
//     ({known $n:ident}) => { $n }; // keep now known (see with_unk_from_x_to_ident)
//     ($n:ident) => { Type::Unk(stringify!($n).to_string()) };
// }

// /// Goes from function DSL type to the Function::maps tuple.
// macro_rules! fun_ty {
//     ($h:tt -> $($t:tt)->+) => {
//         (ty_ty!($h), ty_ty!($($t)->+))
//     };
// }

// /// Makes the match that extracts the value from the
// /// given Option<Value>. $ty is still DSL.
// macro_rules! cast_option {
//     ((Num)$val:expr) => {
//         match $val {
//             Some(Value::Num(v)) => v,
//             _ => unreachable!(),
//         }
//     };
//     ((Str)$val:expr) => {
//         match $val {
//             Some(Value::Str(v)) => v,
//             _ => unreachable!(),
//         }
//     };
//     (([$_t:tt])$val:expr) => {
//         match $val {
//             Some(Value::Arr(v)) => v,
//             _ => unreachable!(),
//         }
//     };
//     ((($_h:tt -> $($_t:tt)->+))$val:expr) => {
//         match $val {
//             Some(Value::Fun(v)) => v,
//             _ => unreachable!(),
//         }
//     };
//     (($_unk:ident)$val:expr) => { $val.unwrap() };
// }

// /// idk. $ty is still DSL.
// macro_rules! make_ret {
//     ($ty_out:expr, (Num)$val:expr) => { Value::Num($val) };
//     ($ty_out:expr, (Str)$val:expr) => { Value::Str($val) };
//     ($ty_out:expr, ([$_t:tt])$val:expr) => {
//         Value::Arr(Array {
//             has: $ty_out,
//             items: $val,
//         })
//     };
//     ($ty_out:expr, ($_unk:ident)$val:expr) => { $val };
// }

// /// Goes from function definition to actual Function struct.
// /// SELF-NOTE: internal recursion goes like:
// ///   (@ name, args ; decr_fn_ty = expr => incr_fn_ty)
// macro_rules! fun_fn {
//     // ($t:tt = $def:expr) => {
//     //     // TODO: non-function values in the prelude (rather a val! macro)
//     // };
//     ($name:ident :: $($t:tt)->+ = $def:expr) => {
//         fun_fn!(@ stringify!($name).to_string(), vec![] ; $($t)->* = $def => )
//     };
//     (@ $name:expr, $args:expr ; $h:tt -> $m:tt -> $($t:tt)->+ = $def:expr => $($x:tt)->*) => {
//         Function {
//             name: $name,
//             maps: fun_ty!($h -> $m -> $($t)->+),
//             args: $args,
//             func: |this| {
//                 extract_now_knowns!(
//                     ($h)this.args.index(0).typed(), { // now in scope: a, b, c...
//                         with_unk_from_x_to_ident!(
//                             @ this.name, this.args ;
//                             $h, ($m -> $($t)->+) // will recurse into fun_fn after updating now knowns
//                             = $def => $($x->)* $h
//                         )
//                     }
//                 )
//             }
//         }
//     };
//     (@ $name:expr, $args:expr ; $a:tt -> $b:tt = $def:expr => $($x:tt)->*) => {
//         Function {
//             name: $name,
//             maps: fun_ty!($a -> $b),
//             args: $args,
//             func: |this| {
//                 let mut args_iter = this.args.into_iter();
//                 make_ret!(
//                     this.maps.1,
//                     ($b)(($def)(
//                         $(cast_option!(($x)args_iter.next()),)*
//                         cast_option!(($a)args_iter.next())
//                     ))
//                 )
//             }
//         }
//     };
// }

// /// Thin wrapper over fun_fn! to adapt easier to whever
// /// the backing structure for the prelude will be.
// macro_rules! fun {
//     ($name:ident :: $($t:tt)->+ = $def:expr) => {
//         (stringify!($name).to_string(), fun_fn!($name :: $($t)->+ = $def))
//     };
// }

macro_rules! named_val {
    ($name:ident :: $($t:tt)->+ = $def:expr) => {
        ( stringify!($name).to_string()
        , stringify!($name :: $($t)->+).to_string()
        , val!($name :: $($t)->+ = $def)
        )
    };
}

fn _crap() -> Vec<(String, String, Value)> {
    // fun!(idk :: (a -> b -> c) -> Num = |f| 42.0);

    // fun_ty!((a -> b) -> Num);

    // fun_fn!(@ stringify!(idk).to_string(), vec![] ; (a -> b) -> Num = |f| 42.0 => );

    // fun_ty!((a -> b) -> Num);

    // cast_option!(($a)args_iter.next())


    vec![

    named_val!(add :: Num -> Num -> Num = |a, b| a + b),
    named_val!(pi :: Num = 3.14),
    named_val!(sum :: [Num] -> Num = |a| 0),

    // named_val!(id :: a -> a = |a| a),

    // named_val!(map :: (a -> b) -> [a] -> [b] = |f: Function, a: Array|
    //     a.items
    //         .into_iter()
    //         .map(|i| f.clone().apply(i))
    //         .collect()
    // ),

    // ("map".to_string(), Function { // (a -> b) -> [a] -> [b]
    //     maps: (
    //         Type::Fun(
    //             Box::new(Type::Unk("a".to_string())),
    //             Box::new(Type::Unk("b".to_string()))
    //         ),
    //         Type::Fun(
    //             Box::new(Type::Arr(Box::new(Type::Unk("a".to_string())))),
    //             Box::new(Type::Arr(Box::new(Type::Unk("b".to_string()))))
    //         )
    //     ),
    //     args: vec![],
    //     func: |_, args| {
    //         let_sure!{ Type::Fun(typea, typeb) = args[0].clone().typed();
    //         Value::Fun(Function { // [a] -> [b]
    //             maps: (
    //                 Type::Arr(typea),
    //                 Type::Arr(typeb)
    //             ),
    //             args,
    //             func: |types, args| {
    //                 let_sure!{ (Type::Arr(typeb), [f, Value::Arr(a)]) = (types.1, &args[..2]);
    //                 Value::Arr(Array { // [b]
    //                     has: *typeb,
    //                     items: a
    //                         .into_iter()
    //                         .map(|i| f.clone().apply(i.clone()))
    //                         .collect()
    //                 }) }
    //             }
    //         }) }
    //     },
    // }),

    // fun!(tonum :: Str -> Num = |s| s.parse().unwrap_or(0.0)),
    /*
    ("tonum".to_string(), Function { // Str -> Num
        maps: (Type::Str, Type::Num),
        args: vec![],
        func: |_, args| {
            let_sure!{ Value::Str(c) = &args[0];
            Value::Num(c.parse().unwrap_or(0.0)) }
        },
    }),
    */

    // fun!(tostr :: Num -> Str = |n| n.to_string()),

    // fun!(pi :: Num = 3.14),

    ]

}

// fn _fart() {
//     let _z = fun_fn!(Num -> Str -> Num = "coucou");
//     // ty_ty!((Str -> Num));
//     // ty_ty!([Str]);
// }
