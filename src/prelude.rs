use lazy_static::lazy_static;
use crate::{engine::{Function, Value, Array, Type, Typed, Apply}, parser::{Binop, Unop}};

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
lazy_static!{ static ref LU: [(&'static str, Function); 11] = [

    ("add", Function { // Num -> Num -> Num
        name: "add".to_string(),
        maps: (
            Type::Num,
            Type::Fun(Box::new(Type::Num), Box::new(Type::Num))
        ),
        args: vec![],
        func: |this| {
            Value::Fun(Function {
                name: this.name,
                maps: (
                    Type::Num,
                    Type::Num
                ),
                args: this.args,
                func: |this| {
                    let_sure!{ [Value::Num(a), Value::Num(b)] = this.args[..2];
                    Value::Num(a+b) }
                }
            })
        },
    }),

    ("const", Function { // a -> b -> a
        name: "const".to_string(),
        maps: (
            Type::Unk("a".to_string()),
            Type::Fun(
                Box::new(Type::Unk("b".to_string())),
                Box::new(Type::Unk("a".to_string()))
            )
        ),
        args: vec![],
        func: |this| {
            Value::Fun(Function {
                name: this.name,
                maps: (
                    Type::Unk("b".to_string()),
                    this.args[0].typed()
                ),
                args: this.args,
                func: |this| this.args[0].clone(),
            })
        },
    }),

    ("id", Function { // a -> a
        name: "id".to_string(),
        maps: (Type::Unk("a".to_string()), Type::Unk("a".to_string())),
        args: vec![],
        func: |this| this.args[0].clone(),
    }),

    ("join", Function { // Str -> [Str] -> Str
        name: "join".to_string(),
        maps: (
            Type::Str,
            Type::Fun(
                Box::new(Type::Arr(Box::new(Type::Str))),
                Box::new(Type::Str)
            )
        ),
        args: vec![],
        func: |this| {
            Value::Fun(Function {
                name: this.name,
                maps: (
                    Type::Arr(Box::new(Type::Str)),
                    Type::Str
                ),
                args: this.args,
                func: |this| {
                    let_sure!{ [Value::Str(c), Value::Arr(a)] = &this.args[..2];
                    Value::Str(a.items
                        .iter()
                        .map(|i| {
                            let_sure!{ Value::Str(s) = i;
                            s.clone() }
                        })
                        .collect::<Vec<String>>()
                        .join(c.as_str())
                    ) }
                }
            })
        }
    }),

    ("map", Function { // (a -> b) -> [a] -> [b]
        name: "map".to_string(),
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
        func: |this| {
            let_sure!{ Type::Fun(typea, typeb) = this.args[0].clone().typed();
            Value::Fun(Function { // [a] -> [b]
                    name: this.name,
                maps: (
                    Type::Arr(typea),
                    Type::Arr(typeb)
                ),
                args: this.args,
                func: |this| {
                    let_sure!{ (Type::Arr(typeb), [f, Value::Arr(a)]) = (this.maps.1, &this.args[..2]);
                    Value::Arr(Array { // [b]
                        has: *typeb,
                        items: a.items
                            .iter()
                            .map(|i| f.clone().apply(i.clone()))
                            .collect()
                    }) }
                }
            }) }
        },
    }),

    ("repeat", Function { // Num -> a -> Str // ok, was 'replicate'
        name: "repeat".to_string(),
        maps: (
            Type::Num,
            Type::Fun(
                Box::new(Type::Unk("a".to_string())),
                Box::new(Type::Arr(Box::new(Type::Unk("a".to_string()))))
            )
        ),
        args: vec![],
        func: |this| {
            Value::Fun(Function {
                name: this.name,
                maps: (
                    Type::Unk("a".to_string()),
                    Type::Arr(Box::new(Type::Unk("a".to_string())))
                ),
                args: this.args,
                func: |this| {
                    let_sure!{ Value::Num(n) = &this.args[0];
                    Value::Arr(Array {
                        has: this.maps.0,
                        items: vec![this.args[1].clone(); *n as usize]
                    }) }
                },
            })
        },
    }),

    ("reverse", Function { // [a] -> [a]
        name: "reverse".to_string(),
        maps: (
            Type::Arr(Box::new(Type::Unk("a".to_string()))),
            Type::Arr(Box::new(Type::Unk("a".to_string())))
        ),
        args: vec![],
        func: |this| {
            let crap = this.args.into_iter().next(); // YYY: interesting
            match crap {
                Some(Value::Arr(v)) => {
                    Value::Arr(Array {
                        has: v.has.clone(),
                        items: v.items
                            .into_iter()
                            .rev()
                            .collect()
                    })
                },
                _ => panic!("type error"),
            }
        },
    }),

    ("singleton", Function { // a -> [a]
        name: "singleton".to_string(),
        maps: (
            Type::Unk("a".to_string()),
            Type::Arr(Box::new(Type::Unk("a".to_string())))
        ),
        args: vec![],
        func: |this| Value::Arr(Array { has: this.args[0].typed(), items: this.args }),
    }),

    ("split", Function { // Str -> Str -> [Str]
        name: "split".to_string(),
        maps: (
            Type::Str,
            Type::Fun(
                Box::new(Type::Str),
                Box::new(Type::Arr(Box::new(Type::Str)))
            )
        ),
        args: vec![],
        func: |this| {
            Value::Fun(Function {
                name: this.name,
                maps: (
                    Type::Str,
                    Type::Arr(Box::new(Type::Str))
                ),
                args: this.args,
                func: |this| {
                    match (&this.args[0], &this.args[1]) {
                        (Value::Str(c), Value::Str(s)) =>
                            Value::Arr(Array {
                                has: Type::Str,
                                items: s
                                    .split(c)
                                    .map(|u| Value::Str(u.to_string()))
                                    .collect()
                            }),
                        _ => panic!("type error"),
                    }
                }
            })
        },
    }),

    ("tonum", Function { // Str -> Num
        name: "tonum".to_string(),
        maps: (Type::Str, Type::Num),
        args: vec![],
        func: |this| {
            let_sure!{ Value::Str(c) = &this.args[0];
            Value::Num(c.parse().unwrap_or(0.0)) }
        },
    }),

    ("tostr", Function { // Num -> Str // should probably be a -> Str
        name: "tostr".to_string(),
        maps: (Type::Num, Type::Str),
        args: vec![],
        func: |this| {
            let_sure!{ Value::Num(a) = &this.args[0];
            Value::Str(a.to_string()) }
        },
    }),

]; }

pub fn lookup_name<'a>(name: String) -> Option<Value> {
    LU
        .binary_search_by_key(&name.as_str(), |t| t.0)
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
    // XXX: they are all flipped
    match bin {
        Binop::Addition       => lookup_name("add".to_string()).unwrap(),
        Binop::Substraction   => lookup_name("sub".to_string()).unwrap(),
        Binop::Multiplication => lookup_name("mul".to_string()).unwrap(),
        Binop::Division       => lookup_name("div".to_string()).unwrap(),
        // Binop::Range          => lookup_name("range".to_string()).unwrap(),
    }
}
