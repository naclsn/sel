use dsl_macro_lib::val;
use std::ops::Index;

use crate::{
    engine::{Apply, Array, Function, Number, Type, Typed, Value},
    parser::{Binop, Unop},
};

pub fn get_prelude() -> impl PreludeLookup {
    _get_prelude_impl()
}

pub struct PreludeEntry {
    pub name: &'static str,
    pub typedef: &'static str,
    pub docstr: &'static str,
    pub value: fn() -> Value,
}

pub trait PreludeLookup {
    fn lookup(&self, name: String) -> Option<&PreludeEntry>;

    fn lookup_name(&self, name: String) -> Option<Value> {
        self.lookup(name).map(|it| (it.value)())
    }

    fn lookup_unary(&self, un: Unop) -> Value {
        match un {
            Unop::Array => self.lookup_name("array".to_string()).unwrap(),
            Unop::Flip => self.lookup_name("flip".to_string()).unwrap(),
        }
    }

    fn lookup_binary(&self, bin: Binop) -> Value {
        // XXX: they are all flipped
        match bin {
            Binop::Addition => self.lookup_name("add".to_string()).unwrap(),
            Binop::Substraction => self.lookup_name("sub".to_string()).unwrap(),
            Binop::Multiplication => self.lookup_name("mul".to_string()).unwrap(),
            Binop::Division => self.lookup_name("div".to_string()).unwrap(),
            // Binop::Range          => self.lookup_name("range".to_string()).unwrap(),
        }
    }
}

macro_rules! make_prelude {
    ($(($name:ident :: $($ty:tt)->+ = $def:expr; $doc:literal)),*,) => {
        struct _Prelude(Vec<PreludeEntry>);

        impl PreludeLookup for _Prelude {
            fn lookup(&self, name: String) -> Option<&PreludeEntry> {
                self.0
                    .binary_search_by_key(&name.as_str(), |t| t.name)
                    .map(|k| &self.0[k])
                    .ok()
            }
        }

        fn _get_prelude_impl() -> _Prelude {
            _Prelude(vec![
                $(make_prelude!(@ $name :: $($ty)->+ = $def; $doc)),*
            ])
        }
    };
    (@ $name:ident :: $($ty:tt)->+ = $def:expr; $doc:literal) => {
        PreludeEntry {
            name: stringify!($name),
            typedef: stringify!($($ty)->+),
            docstr: $doc,
            value: || val!($name :: $($ty)->+ = $def),
        }
    };
}

make_prelude! {
    (add :: Num -> Num -> Num = |a: Number, b: Number| a + b;
        "add two numbers"),
    (const :: a -> b -> a = |a: Value, _: Value| a;
        "always evaluate to its first argument, ignoring its second argument"),
    (map :: (a -> b) -> [a] -> [b] = |f: Function, a: Array|
        Array { // XXX: working with array is not perfect...
            has: a.has,
            items: a.items
                .into_iter()
                .map(|v| f.clone().apply(v))
                .collect(),
        };
        "make a new list by applying a function to each value from a list"),
    (split :: Str -> Str -> [Str] = |sep: String, s: String| s.split(&sep).collect::<Value>();
        "break a string into pieces separated by the argument, consuming the delimiter"),
}

// TODO: using dsl_macro_lib::val
// lazy_static! { static ref LU: [(&'static str, Function); 11] = [

//     ("add", Function { // Num -> Num -> Num
//         name: "add".to_string(),
//         maps: (
//             Type::Num,
//             Type::Fun(Box::new(Type::Num), Box::new(Type::Num))
//         ),
//         args: vec![],
//         func: |this| {
//             Value::Fun(Function {
//                 name: this.name,
//                 maps: (
//                     Type::Num,
//                     Type::Num
//                 ),
//                 args: this.args,
//                 func: |this| {
//                     let_sure!{ [Value::Num(a), Value::Num(b)] = this.args[..2];
//                     Value::Num(a+b) }
//                 }
//             })
//         },
//     }),

//     ("const", Function { // a -> b -> a
//         name: "const".to_string(),
//         maps: (
//             Type::Unk("a".to_string()),
//             Type::Fun(
//                 Box::new(Type::Unk("b".to_string())),
//                 Box::new(Type::Unk("a".to_string()))
//             )
//         ),
//         args: vec![],
//         func: |this| {
//             Value::Fun(Function {
//                 name: this.name,
//                 maps: (
//                     Type::Unk("b".to_string()),
//                     this.args[0].typed()
//                 ),
//                 args: this.args,
//                 func: |this| this.args[0].clone(),
//             })
//         },
//     }),

//     ("id", Function { // a -> a
//         name: "id".to_string(),
//         maps: (Type::Unk("a".to_string()), Type::Unk("a".to_string())),
//         args: vec![],
//         func: |this| this.args[0].clone(),
//     }),

//     ("join", Function { // Str -> [Str] -> Str
//         name: "join".to_string(),
//         maps: (
//             Type::Str,
//             Type::Fun(
//                 Box::new(Type::Arr(Box::new(Type::Str))),
//                 Box::new(Type::Str)
//             )
//         ),
//         args: vec![],
//         func: |this| {
//             Value::Fun(Function {
//                 name: this.name,
//                 maps: (
//                     Type::Arr(Box::new(Type::Str)),
//                     Type::Str
//                 ),
//                 args: this.args,
//                 func: |this| {
//                     let_sure!{ [Value::Str(c), Value::Arr(a)] = &this.args[..2];
//                     Value::Str(a.items
//                         .iter()
//                         .map(|i| {
//                             let_sure!{ Value::Str(s) = i;
//                             s.clone() }
//                         })
//                         .collect::<Vec<String>>()
//                         .join(c.as_str())
//                     ) }
//                 }
//             })
//         }
//     }),

//     ("map", Function { // (a -> b) -> [a] -> [b]
//         name: "map".to_string(),
//         maps: (
//             Type::Fun(
//                 Box::new(Type::Unk("a".to_string())),
//                 Box::new(Type::Unk("b".to_string()))
//             ),
//             Type::Fun(
//                 Box::new(Type::Arr(Box::new(Type::Unk("a".to_string())))),
//                 Box::new(Type::Arr(Box::new(Type::Unk("b".to_string()))))
//             )
//         ),
//         args: vec![],
//         func: |this| {
//             let_sure!{ Type::Fun(typea, typeb) = this.args.index(0).typed();
//             Value::Fun(Function { // [a] -> [b]
//                 name: this.name,
//                 maps: (
//                     Type::Arr(typea),
//                     Type::Arr(typeb)
//                 ),
//                 args: this.args,
//                 func: |this| {
//                     let mut arg_iter = this.args.into_iter();
//                     match (this.maps, (arg_iter.next(), arg_iter.next())) {
//                         ((_, Type::Arr(typeb)), (Some(f), Some(Value::Arr(a)))) =>
//                             Value::Arr(Array { // [b]
//                                 has: *typeb,
//                                 items: a.items
//                                     .into_iter()
//                                     .map(|i| f.clone().apply(i))
//                                     .collect()
//                             }),
//                         _ => unreachable!(),
//                     }
//                 }
//             }) }
//         },
//     }),

//     ("replicate", Function { // Num -> a -> Str // ok, was 'replicate'
//         name: "replicate".to_string(),
//         maps: (
//             Type::Num,
//             Type::Fun(
//                 Box::new(Type::Unk("a".to_string())),
//                 Box::new(Type::Arr(Box::new(Type::Unk("a".to_string()))))
//             )
//         ),
//         args: vec![],
//         func: |this| {
//             Value::Fun(Function {
//                 name: this.name,
//                 maps: (
//                     Type::Unk("a".to_string()),
//                     Type::Arr(Box::new(Type::Unk("a".to_string())))
//                 ),
//                 args: this.args,
//                 func: |this| {
//                     let mut args_iter = this.args.into_iter();
//                     let_sure!{ (Some(Value::Num(n)), Some(o)) = (args_iter.next(), args_iter.next());
//                     Value::Arr(Array {
//                         has: o.typed(),
//                         items: vec![o; n as usize]
//                     }) }
//                 },
//             })
//         },
//     }),

//     ("reverse", Function { // [a] -> [a]
//         name: "reverse".to_string(),
//         maps: (
//             Type::Arr(Box::new(Type::Unk("a".to_string()))),
//             Type::Arr(Box::new(Type::Unk("a".to_string())))
//         ),
//         args: vec![],
//         func: |this| {
//             let crap = this.args.into_iter().next(); // YYY: interesting
//             match crap {
//                 Some(Value::Arr(v)) => {
//                     Value::Arr(Array {
//                         has: v.has,
//                         items: v.items
//                             .into_iter()
//                             .rev()
//                             .collect()
//                     })
//                 },
//                 _ => panic!("type error"),
//             }
//         },
//     }),

//     ("singleton", Function { // a -> [a]
//         name: "singleton".to_string(),
//         maps: (
//             Type::Unk("a".to_string()),
//             Type::Arr(Box::new(Type::Unk("a".to_string())))
//         ),
//         args: vec![],
//         func: |this| Value::Arr(Array { has: this.args[0].typed(), items: this.args }),
//     }),

//     ("split", Function { // Str -> Str -> [Str]
//         name: "split".to_string(),
//         maps: (
//             Type::Str,
//             Type::Fun(
//                 Box::new(Type::Str),
//                 Box::new(Type::Arr(Box::new(Type::Str)))
//             )
//         ),
//         args: vec![],
//         func: |this| {
//             Value::Fun(Function {
//                 name: this.name,
//                 maps: (
//                     Type::Str,
//                     Type::Arr(Box::new(Type::Str))
//                 ),
//                 args: this.args,
//                 func: |this| {
//                     match (&this.args[0], &this.args[1]) {
//                         (Value::Str(c), Value::Str(s)) =>
//                             Value::Arr(Array {
//                                 has: Type::Str,
//                                 items: s
//                                     .split(c)
//                                     .map(|u| Value::Str(u.to_string()))
//                                     .collect()
//                             }),
//                         _ => panic!("type error"),
//                     }
//                 }
//             })
//         },
//     }),

//     ("tonum", Function { // Str -> Num
//         name: "tonum".to_string(),
//         maps: (Type::Str, Type::Num),
//         args: vec![],
//         func: |this| {
//             let_sure!{ Value::Str(c) = &this.args[0];
//             Value::Num(c.parse().unwrap_or(0.0)) }
//         },
//     }),

//     ("tostr", Function { // Num -> Str // should probably be a -> Str
//         name: "tostr".to_string(),
//         maps: (Type::Num, Type::Str),
//         args: vec![],
//         func: |this| {
//             let_sure!{ Value::Num(a) = &this.args[0];
//             Value::Str(a.to_string()) }
//         },
//     }),

// ]; }
