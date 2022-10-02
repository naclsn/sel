use crate::{engine::{Function, Value, Apply, Array, Type, Typed}, parser::{Binop, Unop}};

macro_rules! let_sure {
    ($pat:pat = $ex:expr; $bl:block) => {
        match $ex {
            $pat => $bl,
            _ => unreachable!(),
        }
    };
    ($pat:pat = $ex:expr; $bl:expr) => {
        let_sure!($pat = $ex; { $bl })
    };
}

// macro_rules! count {
//     ($_:expr,) => { 1usize };
//     ($h:expr, $($t:expr,)*) => { 1usize + count!($($t,)*) };
// }

// macro_rules! ty_for {
//     ($simple:ident) => { Type::$simple }
// }

// macro_rules! val_for {
//     ($simple:ident) => { Value::$simple }
// }

// macro_rules! match_for {
//     ($simple:ident, $v:expr) => {
//         match $v {
//             Value::$simple(v) => v,
//             other => panic!("match_ex_val: type mismatch (expected a {:?} got {other:?}", Type::$simple),
//         }
//     }
// }

// macro_rules! fun {
//     ($name:ident :: [$($ty:ident),*] => $re:ident, $fn:expr) => {
//         Fun {
//             name: stringify!($name),
//             params: &[$(ty_for!($ty)),*],
//             ret: ty_for!($re),
//             func: |args| {
//                 assert!(count!($($ty,)*) == args.len());
//                 let mut k: usize = 0;
//                 val_for!($re)($fn($(
//                     match_for!($ty, &args[{ k+= 1; k-1 }])
//                     // match &args[{ k+= 1; k-1 }] {
//                     //     Value::$ty(v) => v,
//                     //     other => panic!("Type mismatch: expected a {:?} got {other:?}", Type::$ty),
//                     // }
//                 ),*))
//             },
//             args: vec![], // alloc..?
//         }
//     };
// } // fun!

// /// then :: (b -> c) -> (a -> b) -> a -> c
// pub(crate) const THEN: Function = Function {
//     arity: 3usize,
//     args: vec![],
//     func: |args| {
//         match (&args[0], &args[1], &args[2]) {
//             (Value::Fun(g), Value::Fun(f), arg) => todo!(),
//             _ => panic!("type error"),
//         }
//     },
// };

// TODO: re-do macro (with doc inline!)
// const LU: &'static [(&'static str, Function)] = &[..]
fn get_lu() -> Vec<(String, Function)> {
    vec![

    ("add".to_string(), Function { // Num -> Num -> Num
        maps: (
            Type::Num,
            Type::Fun(Box::new(Type::Num), Box::new(Type::Num))
        ),
        args: vec![],
        func: |_, args| {
            Value::Fun(Function { // Num -> Num
                maps: (
                    Type::Num,
                    Type::Num
                ),
                args,
                func: |_, args| {
                    let_sure!{ [Value::Num(a), Value::Num(b)] = args[..2];
                    Value::Num(a+b) }
                }
            })
        },
    }),

    ("id".to_string(), Function {
        maps: (Type::Unk("a".to_string()), Type::Unk("a".to_string())),
        args: vec![],
        func: |_, args| args[0].clone(),
    }),

    // ("join".to_string(), Function {
    //     arity: 2usize,
    //     args: vec![],
    //     func: |args| {
    //         match (&args[0], &args[1]) {
    //             (Value::Str(c), Value::Arr(a)) =>
    //                 Value::Str(a
    //                     .into_iter()
    //                     .map(|i| {
    //                         match i {
    //                             Value::Str(s) => s.clone(),
    //                             _ => panic!("type error"),
    //                         }
    //                     })
    //                     .collect::<Vec<String>>()
    //                     .join(c)
    //                 ),
    //             _ => panic!("type error"),
    //         }
    //     }
    // }),

    ("map".to_string(), Function { // (a -> b) -> [a] -> [b]
        maps: (
            Type::Fun(
                Box::new(Type::Unk("a".to_string())),
                Box::new(Type::Unk("b".to_string()))
            ),
            Type::Fun(
                Box::new(Type::Arr(Box::new(Type::Unk("a".to_string())))),
                Box::new(Type::Arr(Box::new(Type::Unk("b".to_string()))))
            )
        ),
        args: vec![],
        func: |_, args| {
            let_sure!{ Type::Fun(typea, typeb) = args[0].clone().typed();
            Value::Fun(Function { // [a] -> [b]
                maps: (
                    Type::Arr(typea),
                    Type::Arr(typeb)
                ),
                args,
                func: |types, args| {
                    let_sure!{ (Type::Arr(typeb), [f, Value::Arr(a)]) = (types.1, &args[..2]);
                    Value::Arr(Array { // [b]
                        has: *typeb,
                        items: a
                            .into_iter()
                            // and here could typec every i to be of typea (but redundant)
                            .map(|i| f.clone().apply(i.clone()))
                            .collect()
                    }) }
                }
            }) }
        },
    }),

    // ("repeat".to_string(), Function {
    //     arity: 2usize,
    //     args: vec![],
    //     func: |args| {
    //         match &args[0] {
    //             Value::Num(n) =>
    //                 Value::Arr(Array::new(
    //                     Type::Num, // YYY: &args[0].typed() -- to infer
    //                     vec![args[1].clone(); *n as usize])),
    //             _ => panic!("type error"),
    //         }
    //     },
    // }),

    // ("reverse".to_string(), Function {
    //     arity: 1usize,
    //     args: vec![],
    //     func: |args| {
    //         match &args[0] {
    //             Value::Arr(v) =>
    //                 Value::Arr(Array::new(
    //                     v.types,
    //                     v
    //                         .into_iter()
    //                         .map(|it| it.clone())
    //                         .rev()
    //                         .collect())),
    //             _ => panic!("type error"),
    //         }
    //     },
    // }),

    // ("singleton".to_string(), Function {
    //     arity: 1usize,
    //     args: vec![],
    //     func: |args| Value::Arr(Array::new(args[0].typed(), args)),
    // }),

    // ("split".to_string(), Function {
    //     arity: 2usize,
    //     args: vec![],
    //     func: |args| {
    //         match (&args[0], &args[1]) {
    //             (Value::Str(c), Value::Str(s)) =>
    //                 Value::Arr(Array::new(
    //                     Type::Str,
    //                     s
    //                         .split(c)
    //                         .map(|u| Value::Str(u.to_string()))
    //                         .collect())),
    //             _ => panic!("type error"),
    //         }
    //     },
    // }),

    // ("tonum".to_string(), Function {
    //     arity: 1usize,
    //     args: vec![],
    //     func: |args| {
    //         match &args[0] {
    //             Value::Str(c) => Value::Num(c.parse().unwrap_or(0.0)),
    //             _ => panic!("type error"),
    //         }
    //     },
    // }),

    // ("tostr".to_string(), Function {
    //     arity: 1usize,
    //     args: vec![],
    //     func: |args| {
    //         match &args[0] {
    //             Value::Num(a) => Value::Str(a.to_string()),
    //             _ => panic!("type error"),
    //         }
    //     },
    // }),

]
}

pub fn lookup_name<'a>(name: String) -> Option<Value> {
    // XXX/TODO: move back to const asap
    let LU = get_lu();
    LU
        .binary_search_by_key(&name, |t| t.0.clone())
        .map(|k| LU[k].1.clone())
        .map(|f| Value::Fun(f))
        .ok()
}

pub fn lookup_unary(un: Unop) -> Value {
    match un {
        Unop::Array => lookup_name("array".to_string()).unwrap(),
        Unop::Flip  => lookup_name("flip".to_string()).unwrap(),
    }
}

pub fn lookup_binary(bin: Binop) -> Value {
    match bin {
        Binop::Addition       => lookup_name("add".to_string()).unwrap(),
        Binop::Substraction   => lookup_name("sub".to_string()).unwrap(),
        Binop::Multiplication => lookup_name("mul".to_string()).unwrap(),
        Binop::Division       => lookup_name("div".to_string()).unwrap(),
        Binop::Range          => lookup_name("range".to_string()).unwrap(),
    }
}
