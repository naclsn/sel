use dsl_macro_lib::val;
use std::{iter, ops::Index};

use crate::{
    engine::{Function, List, Number, Type, Value},
    parser::{Binop, Unop},
};

pub struct PreludeEntry {
    pub name: &'static str,
    pub typedef: &'static str,
    pub docstr: &'static str,
    pub value: fn() -> Value,
}

pub trait PreludeLookup {
    fn list(&self) -> Vec<&PreludeEntry>;
    fn lookup(&self, name: String) -> Option<&PreludeEntry> {
        self.list().into_iter().find(|it| it.name == name)
    }

    fn lookup_name(&self, name: String) -> Option<Value> {
        self.lookup(name).map(|it| (it.value)())
    }

    fn lookup_unary(&self, un: Unop) -> Function {
        match un {
            Unop::Array => self.lookup_name("array".to_string()).unwrap(),
            Unop::Flip => self.lookup_name("flip".to_string()).unwrap(),
        }
        .into()
    }

    fn lookup_binary(&self, bin: Binop) -> Function {
        let flip: Function = self.lookup_name("flip".to_string()).unwrap().into();
        flip.apply(
            match bin {
                Binop::Addition => self.lookup_name("add".to_string()).unwrap(),
                Binop::Substraction => self.lookup_name("sub".to_string()).unwrap(),
                Binop::Multiplication => self.lookup_name("mul".to_string()).unwrap(),
                Binop::Division => self.lookup_name("div".to_string()).unwrap(),
                // Binop::Range          => self.lookup_name("range".to_string()).unwrap(),
            }
            .into(),
        )
        .into()
    }
}

macro_rules! make_prelude {
    ($(($name:ident :: $($ty:tt)->+ = $def:expr; $doc:literal)),*,) => {
        #[doc(hidden)]
        struct _Prelude(Vec<PreludeEntry>);

        impl PreludeLookup for _Prelude {
            fn list(&self) -> Vec<&PreludeEntry> {
                self.0.iter().collect()
            }

            fn lookup(&self, name: String) -> Option<&PreludeEntry> {
                self.0
                    .binary_search_by_key(&name.as_str(), |t| t.name)
                    .map(|k| &self.0[k])
                    .ok()
            }
        }

        $(#[doc = concat!("* `", stringify!($name :: $($ty)->+), "` ", $doc, "\n")])*
        pub fn get_prelude() -> impl PreludeLookup {
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
    (add :: Num -> Num -> Num = |a: Number, b: Number|
        a + b;
        "add two numbers"
    ),
    (const :: a -> b -> a = |a: Value, _: Value|
        a;
        "always evaluate to its first argument, ignoring its second argument"
    ),
    (flip :: (a -> b -> c) -> b -> a -> c = |f: Function, snd: Value, fst: Value| {
            let g: Function = f.apply(fst).into();
            g.apply(snd)
        };
        "flip the two parameters by passing the first given after the second one"
    ),
    (id :: a -> a = |a: Value|
        a;
        "the identity function, returns its input"
    ),
    (join :: Str -> [Str] -> Str = |sep: String, a: Vec<String>|
        a.join(sep.as_str());
        "join a list of string with a separator between entries"
    ),
    (map :: (a -> b) -> [a] -> [b] = |f: Function, a: List|
        List::new(f.maps.1.clone(), a.into_iter().map(|v| f.clone().apply(v)));
        "make a new list by applying a function to each value from a list"
    ),
    (repeat :: a -> [a] = |a: Value|
        List::new(a.typed(), iter::repeat(a));
        "repeat an infinite amount of copies of the same value (as of now, this simply crashes)"
    ),
    (replicate :: Num -> a -> [a] = |n: Number, a: Value|
        List::new(a.typed(),iter::repeat(a).take(n as usize));
        "replicate a finite amount of copies of the same value"
    ),
    (reverse :: [a] -> [a] = |a: List|
        List::new(a.has.clone(), a.into_iter().rev());
        "reverse the order of the elements in the list"
    ),
    (singleton :: a -> [a] = |a: Value|
        List::new(a.typed(), iter::once(a));
        "make a list of an item"
    ),
    (split :: Str -> Str -> [Str] = |sep: String, s: String|
        s.split(&sep).collect::<Value>();
        "break a string into pieces separated by the argument, consuming the delimiter"
    ),
    (sub :: Num -> Num -> Num = |a: Number, b: Number|
        a-b;
        "substract the second number from the first"
    ),
    (take :: Num -> [a] -> [a] = |n: Number, a: List|
        List::new(a.has.clone(), a.into_iter().take(n as usize));
        "take the first elements of a list, discading the rest"
    ),
    (tonum :: Str -> Num = |s: String|
        s.parse::<Number>().unwrap_or(0.0);
        "convert a string into number"
    ),
    (tostr :: Num -> Str = |n: String|
        n.to_string();
        "convert a number into string"
    ),
}
