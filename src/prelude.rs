use crate::{engine::{Function, Value}, parser::{Binop, Unop}};

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
const LU: &'static [(&'static str, Function)] = &[
    ("add", Function {
        arity: 2usize,
        args: vec![],
        func: |args| {
            match (&args[0], &args[1]) {
                (Value::Num(a), Value::Num(b)) => Value::Num(a+b),
                _ => panic!("type error"),
            }
        },
    }),
    ("id", Function { // YYY: remove (or disallow [])
        arity: 1usize,
        args: vec![],
        func: |args| args[0].clone(),
    }),
    ("repeat", Function {
        arity: 2usize,
        args: vec![],
        func: |args| {
            match &args[0] {
                Value::Num(n) => Value::Arr(vec![args[1].clone(); *n as usize]),
                _ => panic!("type error"),
            }
        },
    }),
    ("reverse", Function {
        arity: 1usize,
        args: vec![],
        func: |args| {
            match &args[0] {
                Value::Arr(v) => Value::Arr(v
                    .iter()
                    .map(|it| it.clone())
                    .rev()
                    .collect()
                ),
                _ => panic!("type error"),
            }
        },
    }),
    ("singleton", Function {
        arity: 1usize,
        args: vec![],
        func: |args| Value::Arr(args),
    }),
    ("tonum", Function {
        arity: 1usize,
        args: vec![],
        func: |args| {
            match &args[0] {
                Value::Str(c) => Value::Num(c.parse().unwrap_or(0.0)),
                _ => panic!("type error"),
            }
        },
    }),
    ("tostr", Function {
        arity: 1usize,
        args: vec![],
        func: |args| {
            match &args[0] {
                Value::Num(a) => Value::Str(a.to_string()),
                _ => panic!("type error"),
            }
        },
    }),
];

pub fn lookup_name<'a>(name: String) -> Option<Value> {
    LU
        .binary_search_by_key(&name.as_str(), |t| t.0)
        .map(|k| LU[k].1.clone())
        .map(|f| Value::Fun(Box::new(f)))
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
