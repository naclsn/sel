use std::{fmt, slice, vec};

// NOTE: `Debug` is kept for rust-level debugging,
//       `Display` is used for sel-level debugging
//       as such, the actual output for a type of
//       value is through Value::as_text

#[derive(Clone)] //Debug)]
pub enum Type {
    Num,
    Str,
    Lst(Box<Type>),
    Fun(Box<Type>, Box<Type>),
    Unk(String),
}

#[derive(Clone)] //Debug)]
pub enum Value {
    Num(Number),
    Str(String),
    Lst(List),
    Fun(Function),
}

pub type Number = f32;

// YYY: more private fields
#[derive(Clone)] //Debug)]
pub struct List {
    pub has: Type,
    items: Box<dyn Some>,
} // @see: https://stackoverflow.com/questions/30353462/how-to-clone-a-struct-storing-a-boxed-trait-object

// type Idk = Iterator<Item = Value>;

trait Some: SomeClone {}
trait SomeClone { fn clone_box(&self) -> Box<dyn Some>; }

impl<T> SomeClone for T
where
    T: 'static + Some + Clone,
{
    fn clone_box(&self) -> Box<dyn Some> {
        Box::new(self.clone())
    }
}
impl Clone for Box<dyn Some> {
    fn clone(&self) -> Box<dyn Some> {
        self.clone_box()
    }
}

impl<T> Some for T
where
    T: Iterator<Item = Value> + Clone + 'static,
{}

// YYY: more private fields
#[derive(Clone)]
pub struct Function {
    pub name: String,
    pub maps: (Type, Type),
    pub args: Vec<Value>,            // YYY: would like it private
    pub func: fn(Function) -> Value, // YYY: would like it private (maybe `fn(Function, Vec<Value>) -> Value`)
}

impl List {
    // pub fn new<T: Iterator<Item = Value>>(has: Type, items: T) -> List {
    pub fn new(has: Type, items: impl Iterator<Item = Value> + Clone) -> List {
        List { has, items: Box::new(items) }
    }
}

impl Iterator for List {
    type Item = Value;

    fn next(&mut self) -> Option<Self::Item> {
        self.items.next()
    }
}

impl Function {
    pub fn apply(mut self, arg: Value) -> Value {
        let given_type = arg.typed();

        match arg.coerse(self.maps.0.clone()) {
            None => {
                println!("wrong type of argument for function:");
                println!("    applying argument: {}", given_type);
                println!("  to function mapping: {} to {}", self.maps.0, self.maps.1);
                println!("no valid implicit conversion between the two types");
                panic!("type error");
            }

            Some(correct) => {
                self.args.push(correct);
                (self.func)(self)
            }
        }
    }
}

impl Value {
    pub fn as_text(&self) -> String {
        // ZZZ: consumes, so cannot borrow
        match self {
            Value::Num(n) => n.to_string(),
            Value::Str(s) => s.to_string(),
            Value::Lst(a) => match &a.has {
                Type::Num | Type::Str | Type::Unk(_) => a // XXX: list of unk
                    .iter()
                    .map(Value::as_text)
                    .collect::<Vec<String>>() // ZZZ: inf hang
                    .join(" "),
                Type::Lst(t) => match **t {
                    Type::Num | Type::Str | Type::Unk(_) => a // XXX: list of unk
                        .iter()
                        .map(Value::as_text)
                        .collect::<Vec<String>>() // ZZZ: inf hang
                        .join("\n"),
                    _ => {
                        panic!("no acceptable text conversion for the last return type (deep list)")
                    }
                },
                _ => panic!(
                    "no acceptable text conversion for the last return type (list of functions)"
                ),
            },
            _ => panic!("no acceptable text conversion for the last return type (function)"),
        }
    }

    pub fn typed(&self) -> Type {
        match self {
            Value::Num(_) => Type::Num,
            Value::Str(_) => Type::Str,
            Value::Lst(a) => Type::Lst(Box::new(a.has.clone())),
            Value::Fun(f) => Type::Fun(Box::new(f.maps.0.clone()), Box::new(f.maps.1.clone())),
        }
    }

    pub fn coerse(self, to: Type) -> Option<Value> {
        if to == self.typed() {
            return Some(self.clone());
        }
        match to {
            Type::Num => match self {
                Value::Str(v) => v.parse().ok().map(|r| Value::Num(r)),
                _ => None,
            },
            Type::Str => match self {
                Value::Num(v) => Some(Value::Str(v.to_string())),
                _ => None,
            },
            Type::Lst(t) if Type::Num == *t => match self {
                Value::Str(v) => Some(Value::Lst(List::new(
                    Type::Num,
                    v.chars().map(|c| Value::Num((c as u32) as Number)),
                ))),
                Value::Lst(v) => Some(Value::Lst(List::new(
                    *t.clone(),
                    v.into_iter().map(|it| it.coerse_or_keep(Type::Num)),
                ))),

                _ => None,
            },
            Type::Lst(t) if Type::Str == *t => match self {
                Value::Str(v) => Some(Value::Lst(List::new(
                    Type::Str,
                    v.chars().map(|c| Value::Str(c.to_string())),
                ))),
                Value::Lst(v) => Some(Value::Lst(List::new(
                    *t.clone(),
                    v.into_iter().map(|it| it.coerse_or_keep(Type::Str)),
                ))),
                _ => None,
            },
            Type::Lst(t) => match self {
                Value::Lst(v) => Some(Value::Lst(List::new(
                    *t.clone(),
                    v.into_iter().map(|it| it.coerse_or_keep(*t.clone())),
                ))),
                _ => None,
            },
            _ => None,
        } // match to
    } // fn coerse

    /*pub?*/
    fn coerse_or_keep(self, to: Type) -> Value {
        let backup = self.clone(); // could avoid this by failing early
        self.coerse(to).unwrap_or(backup)
    }
}

impl PartialEq for Type {
    /// Two types are equal iif:
    /// - exact same simple type (Num, Str)
    /// - recursively same type (Arr, Fun)
    /// - same type name (Unk)
    /// - only one is unknown
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Type::Num, Type::Num) => true,
            (Type::Str, Type::Str) => true,
            (Type::Lst(a), Type::Lst(b)) => a == b,
            (Type::Fun(a, c), Type::Fun(b, d)) => a == b && c == d,
            (Type::Unk(a), Type::Unk(b)) => a == b, // YYY: ?
            (Type::Unk(_), _) => true,
            (_, Type::Unk(_)) => true,
            _ => false,
        }
    }
}

impl fmt::Display for Type {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Type::Num => write!(f, "Num"),
            Type::Str => write!(f, "Str"),
            Type::Lst(a) => write!(f, "[{a}]"),
            Type::Fun(a, b) => match **a {
                Type::Fun(_, _) => write!(f, "({a}) -> {b}"),
                _ => write!(f, "{a} -> {b}"),
            },
            Type::Unk(n) => write!(f, "{n}"),
        }
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Value::Num(n) => write!(f, "{n}"),
            Value::Str(s) => write!(f, "{{{s}}}"),
            Value::Lst(a) => write!(
                f,
                "@{{{}}}",
                a.iter()
                    .map(|v| match v {
                        Value::Str(s) => s.clone(),
                        Value::Lst(_) => v.to_string()[1..].to_string(),
                        _ => v.to_string(),
                    })
                    .collect::<Vec<String>>()
                    .join(", ")
            ),
            Value::Fun(g) => {
                if 0 == g.args.len() {
                    write!(f, "{}", g.name)
                } else {
                    write!(
                        f,
                        "{} {}",
                        g.name,
                        g.args
                            .iter()
                            .map(|v| {
                                match v {
                                    Value::Fun(h) if 0 < h.args.len() => format!("[{v}]"),
                                    _ => format!("{v}"),
                                }
                            })
                            .collect::<Vec<String>>()
                            .join(" ")
                    )
                }
            }
        }
    }
}

impl fmt::Debug for Function {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "Function({:?}, {:?}, {:?}, {:?})",
            self.name, self.maps, self.args, self.func
        )
    }
}

macro_rules! impl_from_into_value {
    ($type:ident <-> $other:ty: from: $from:expr; into: $into:expr;) => {
        impl From<$other> for Value {
            fn from(o: $other) -> Self {
                Value::$type(($from)(o))
            }
        }
        impl From<Value> for $other {
            fn from(v: Value) -> Self {
                match v {
                    Value::$type(u) => ($into)(u),
                    _ => unreachable!(),
                }
            }
        }
    };
}

/*macro_rules! impl_from_into_iter {
    ([$type:ident] <-> $other:ty: from: $from:expr; into: $into:expr;) => {
        impl FromIterator<$other> for Value {
            fn from_iter<T: IntoIterator<Item = $other>>(iter: T) -> Self {
                Value::Lst(List {
                    has: Type::$type,
                    items: ($from)(iter),
                })
            }
        }
        impl IntoIterator for Value {
            type Item = $other;
            type IntoIter = T
                where T: Iterator<Item=Self::Item>; // pain in the but
            fn into_iter(self) -> Self::IntoIter {
                match self {
                    Value::Lst(u) if Type::$type == u.has => ($into)(u.items),
                    _ => unreachable!(),
                }
            }
        }
    }
}*/

impl_from_into_value! { Num <-> Number:
    from: |o| o;
    into: |u| u;
}

impl_from_into_value! { Str <-> String:
    from: |o| o;
    into: |u| u;
}
impl_from_into_value! { Str <-> &str:
    from: |o: &str| o.to_string();
    into: |_u: String| todo!("&str from String");
}

impl_from_into_value! { Lst <-> Vec<Number>:
    from: |o: Vec<Number>| List::new(Type::Num, o.into_iter().map(Value::from));
    into: |u: List| u.into_iter().map(Number::from).collect();
}
impl_from_into_value! { Lst <-> Vec<String>:
    from: |o: Vec<String>| List::new(Type::Num, o.into_iter().map(Value::from));
    into: |u: List| u.into_iter().map(String::from).collect();
}
impl_from_into_value! { Lst <-> Vec<&str>:
    from: |o: Vec<&str>| List::new(Type::Num, o.into_iter().map(Value::from));
    into: |u: List| u.into_iter().map(|i| i.into()).collect();
}

impl_from_into_value! { Lst <-> List:
    from: |o| o;
    into: |u| u;
}

impl_from_into_value! { Fun <-> Function:
    from: |o| o;
    into: |u| u;
}

// impl FromIterator<Value> for Value {
//     fn from_iter<T: IntoIterator<Item = Value>>(_iter: T) -> Self {
//         todo!() // XXX: still will not be able to infer the contained type!
//         // see discusion about it next commented-out 'From'
//     }
// }

impl FromIterator<Number> for Value {
    fn from_iter<T: IntoIterator<Item = Number>>(iter: T) -> Self {
        Value::Lst(List::new(Type::Num, iter.into_iter().map(Value::from)))
    }
}

impl FromIterator<String> for Value {
    fn from_iter<T: IntoIterator<Item = String>>(iter: T) -> Self {
        Value::Lst(List::new(Type::Str, iter.into_iter().map(Value::from)))
    }
}

impl<'a> FromIterator<&'a str> for Value {
    fn from_iter<T: IntoIterator<Item = &'a str>>(iter: T) -> Self {
        Value::Lst(List::new(Type::Str, iter.into_iter().map(Value::from)))
    }
}

impl IntoIterator for Value {
    type Item = Value;
    type IntoIter = ListIntoIter;

    fn into_iter(self) -> Self::IntoIter {
        match self {
            Value::Lst(u) => u.into_iter(),
            _ => unreachable!(),
        }
    }
}

// impl From<Vec<Value>> for Value {
//     fn from(_: Vec<Value>) -> Self {
//         todo!() // XXX: still will not be able to infer the contained type!
//         // could: if vec not empty, infer type from content
//         //        else use a wildcard type (Unk?) in a way that it does not matter
//         // note that it could simply be `has: Type::Unk("")` and rely on coersion
//         // when the vector is indeed empty...
//         // a question is does using a place-holder type fundamentaly breaks something else...
//     }
// }
impl From<Value> for Vec<Value> {
    fn from(v: Value) -> Self {
        match v {
            Value::Lst(a) => a.into_iter().collect(),
            _ => unreachable!(),
        }
    }
}

impl<const L: usize> From<[Number; L]> for Value {
    fn from(o: [Number; L]) -> Self {
        Value::Lst(List::new(Type::Num, o.into_iter().map(Value::from)))
    }
}

impl<const L: usize> From<[String; L]> for Value {
    fn from(o: [String; L]) -> Self {
        Value::Lst(List::new(Type::Str, o.into_iter().map(Value::from)))
    }
}
