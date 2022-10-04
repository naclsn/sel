use crate::engine::{Apply, Array, Function, Type, Typed, Value};

/// Goes from type representation to actual Type instance.
macro_rules! ty_ty {
    (($t:tt)) => { ty_ty!($t) };
    // (($a:tt -> $b:tt)) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($b))) };
    (($a:tt -> $($b:tt)->+)) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($($b)->+))) };
    (Num) => { Type::Num };
    (Str) => { Type::Str };
    ([$t:tt]) => { Type::Arr(Box::new(ty_ty!($t))) };
    ($a:tt -> $($b:tt)->+) => { Type::Fun(Box::new(ty_ty!($a)), Box::new(ty_ty!($($b)->+))) };
    ($n:ident) => { Type::Unk(stringify!($n).to_string()) };
}

/// Goes from type representation to matches for a Value.
macro_rules! match_ty_val {
    (Num, $v:ident) => { Value::Num(v) };
    (Str, $v:ident) => { Value::Str(v) };
    // ($t:tt, $into:ident) => { $t(v) };
}

/// Goes from function type to the Function::maps tuple.
macro_rules! fun_ty {
    ($a:tt -> $b:tt) => {
        (ty_ty!($a), ty_ty!($b))
    };
    ($h:tt -> $($t:tt)->+) => {
        (ty_ty!($h), ty_ty!($($t)->+))
    };
}

/// Makes the match that extracts the value from the iterator
/// (iterator must be given otherwise 'not found in scope').
macro_rules! make_arg {
    ($args_iter:ident, $ty:tt) => {
        match $args_iter.next() {
            Some(match_ty_val!($ty, v)) => v, // XXX: still 'not found in scope', so no ways to use `match_ty_val`?
            _ => unreachable!(),
        }
    };
}

/// Goes from function definition to actual Function struct.
/// SELF-NOTE: internal recursion goes like:
///   (@ name, args ; decr_fn_ty = expr => incr_fn_ty)
macro_rules! fun_fn {
    // ($t:tt = $def:expr) => {
    //     // TODO: non-function values in the prelude (rather a val! macro)
    // };
    ($name:ident :: $($t:tt)->+ = $def:expr) => {
        fun_fn!(@ stringify!($name).to_string(), vec![] ; $($t)->* = $def => )
    };
    (@ $name:expr, $args:expr ; $h:tt -> $m:tt -> $($t:tt)->+ = $def:expr => $($x:tt)->*) => {
        Function {
            name: $name,
            maps: fun_ty!($h -> $m -> $($t)->+),
            args: $args,
            func: |this| {
                // TODO: args is type checked, extract and fill `Unk`s
                Value::Fun(fun_fn!(@ this.name, this.args ; $m -> $($t)->+ = $def => $($x->)* $h))
            }
        }
    };
    (@ $name:expr, $args:expr ; $a:tt -> $b:tt = $def:expr => $($x:tt)->*) => {
        Function {
            name: $name,
            maps: fun_ty!($a -> $b),
            args: $args,
            func: |this| {
                let mut args_iter = this.args.into_iter();
                let (ty_in, ty_out) = this.maps;
                // YYY: every args could be unwrapped in single match (which might be more better)
                //      for this, needs a macro 'identify!($a:tt)'
                {let _ = (ty_in, ty_out);} // YYY: prevents warnings about unused
                ($def)(
                    $(make_arg!(args_iter, $x),)*
                    make_arg!(args_iter, $a)
                )
            }
        }
    };
}

/// Thin wrapper over fun_fn! to adapt easier to whever
/// the backing structure for the prelude will be.
macro_rules! fun {
    ($name:ident :: $($t:tt)->+ = $def:expr) => {
        (stringify!($name).to_string(), fun_fn!($name :: $($t)->+ = $def))
    };
}

fn _crap() -> Vec<(String, Function)> {
    vec![

    fun!(add :: Num -> Num -> Num = |a, b| a + b),
    // fun!(add :: Num -> Num -> Num = |a, b| match (a, b) {
    //     (Value::Num(aa), Value::Num(bb)) => Value::Num(aa+bb),
    //     _ => unreachable!(),
    // }),
    // fun!(id :: a -> a = |a| a),



    //     a   .into_iter()
    //         .map(|i| f
    //             .clone()
    //             .apply(i.clone()))
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
