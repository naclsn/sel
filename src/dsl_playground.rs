use crate::engine::{Function, Type, Value, Array, Typed, Apply};

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

/// goes from type representation to actual Type instance
macro_rules! ty_ty {
    (($t:tt)) => { ty_ty!($t) };
    (($a:tt -> $b:tt)) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($b))) };
    (Num) => { Type::Num };
    (Str) => { Type::Str };
    ([$t:tt]) => { Type::Arr(Box::new(ty_ty!($t))) };
    ($a:tt -> $b:tt) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($b))) };
    ($n:ident) => { Type::Unk(stringify!($n).to_string()) };
}

/// goes from type representation to actual Type instance
macro_rules! ty_val {
    (($t:tt), $def:expr) => { ty_ty!($t) };
    (($a:tt -> $b:tt), $def:expr) => { Value::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($b))) };
    (Num, $def:expr) => { Value::Num($def) };
    (Str, $def:expr) => { Value::Str($def) };
    ([$t:tt], $def:expr) => { Value::Arr(Box::new(ty_ty!($t))) };
    ($a:tt -> $b:tt, $def:expr) => { Value::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($b))) };
    ($n:ident, $def:expr) => { Value::Unk(stringify!($n).to_string()) };
}

/// goes from function type to the Function::maps tuple
/// YYY: maybe could change said tuple to be a Type..
macro_rules! fun_ty {
    ($a:tt -> $b:tt) => {
        (ty_ty!($a), ty_ty!($b))
    };
    ($h:tt -> $($t:tt)->*) => {
        (ty_ty!($h), ty_ty!($($t)->*))
    };
}

/// goes from function definition to actual Function struct
macro_rules! fun_fn {
    ($t:tt = $def:expr) => {
        ty_val!($t, $def)
    };
    ($($t:tt)->+ = $def:expr) => {
        fun_fn!(@ vec![] ; $($t)->* = $def)
    };
    (@ $args:expr ; $h:tt -> $m:tt -> $($t:tt)->+ = $def:expr) => {
        Function {
            maps: fun_ty!($h -> $m -> $($t)->+),
            args: $args,
            func: |types, args| {
                {let _ = types;}
                // TODO: args is type checked, extract and fill `Unk`s
                Value::Fun(fun_fn!(@ args ; $m -> $($t)->+ = $def))
            }
        }
    };
    (@ $args:expr ; $a:tt -> $b:tt = $def:expr) => {
        Function {
            maps: fun_ty!($a -> $b),
            args: $args,
            func: |types, args| {
                {let _ = (types, args);}
                // TODO: args is type checked, curry and pass to $def
                Value::Num(42.0)
            }
        }
    };
}

macro_rules! fun {
    ($name:ident :: $($t:tt)->* = $def:expr) => {
        (stringify!($name).to_string(), fun_fn!($($t)->* = $def))
    };
}

fn _crap() -> Vec<(String, Function)> {
    vec![

    fun!(add :: Num -> Num -> Num = |a, b| a + b),
    // ("add".to_string(), Function { // Num -> Num -> Num
    //     maps: (
    //         Type::Num,
    //         Type::Fun(Box::new(Type::Num), Box::new(Type::Num))
    //     ),
    //     args: vec![],
    //     func: |_, args| {
    //         Value::Fun(Function {
    //             maps: (
    //                 Type::Num,
    //                 Type::Num
    //             ),
    //             args,
    //             func: |_, args| {
    //                 let_sure!{ [Value::Num(a), Value::Num(b)] = args[..2];
    //                 Value::Num(a+b) }
    //             }
    //         })
    //     },
    // }),

    fun!(id :: a -> a = |a| a),
    /*
    ("id".to_string(), Function { // a -> a
        maps: (Type::Unk("a".to_string()), Type::Unk("a".to_string())),
        args: vec![],
        func: |_, args| args[0].clone(),
    }),
    */

    fun!(map :: (a -> b) -> [a] -> [b] = |f, a|
        a   .into_iter()
            .map(|i| f
                .clone()
                .apply(i.clone()))
            .collect()
    ),
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

    fun!(tonum :: Str -> Num = |s| s.parse().unwrap_or(0.0)),
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

    fun!(tostr :: Num -> Str = |n| n.to_string()),

    fun!(pi :: Num = 3.14),

    ]

}

fn _fart() {
    let _z = fun_fn!(Num -> Str -> Num = "coucou");
    // ty_ty!((Str -> Num));
    // ty_ty!([Str]);
}
