use std::{fmt, vec};

// NOTE: `Debug` is kept for rust-level debugging,
//       `Display` is used for sel-level debugging
//       as such, the actual output for a type of
//       value is through Value::as_text

#[derive(Debug, Clone)]
pub enum Type {
    Num,
    Str,
    Lst(Box<Type>),
    Fun(Box<Type>, Box<Type>),
    Unk(String),
}

#[derive(Debug, Clone)]
pub enum Value {
    Num(Number),
    Str(String),
    Lst(List),
    Fun(Function),
}

pub type Number = f32;

// YYY: more private fields
#[derive(Debug, Clone)]
pub struct List {
    pub has: Type,
    items: Vec<Value>,
}

// YYY: more private fields
#[derive(Clone)]
pub struct Function {
    pub name: String,
    pub maps: (Type, Type),
    pub args: Vec<Value>,            // YYY: would like it private
    pub func: fn(Function) -> Value, // YYY: would like it private (maybe `fn(Function, Vec<Value>) -> Value`)
}

impl List {
    pub fn new<T: Iterator<Item = Value>>(has: Type, items: T) -> List {
        List {
            has,
            items: items.collect(), // XXX: fichtre :-( this is so frustrating
        }
    }
}

impl IntoIterator for List {
    type Item = Value;
    type IntoIter = vec::IntoIter<Value>;
    fn into_iter(self) -> Self::IntoIter {
        self.items.into_iter()
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
    // REM: if ever lists are made lazy, it will be possible to send
    // an infinite data structure here; when that is the case, it would
    // be better to still print things out (ie. iterate the structure
    // rather than collect it) - although this is probably a rare need
    // and could probably do with a simple crash with error message
    pub fn as_text(self) -> String {
        match self {
            Value::Num(n) => n.to_string(),
            Value::Str(s) => s.to_string(),
            Value::Lst(a) => match &a.has {
                Type::Num | Type::Str | Type::Unk(_) => a
                    .into_iter()
                    .map(Value::as_text)
                    .collect::<Vec<String>>()
                    .join(" "),
                Type::Lst(t) => match **t {
                    Type::Num | Type::Str | Type::Unk(_) => a
                        .into_iter()
                        .map(Value::as_text)
                        .collect::<Vec<String>>()
                        .join("\n"),
                    _ => panic!("no text conversion for the last return type (deep list)"),
                },
                _ => panic!("no text conversion for the last return type (functions)"),
            },
            _ => panic!("no text conversion for the last return type (function)"),
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
        let backup = self.clone(); // could avoid this by failing/succeeding early
        self.coerse(to).unwrap_or(backup)
    }
}

impl PartialEq for Type {
    /// Two types are equal iif:
    /// - exact same simple type (Num, Str)
    /// - recursively same type (Arr, Fun)
    /// - either unknown
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Type::Num, Type::Num) => true,
            (Type::Str, Type::Str) => true,
            (Type::Lst(a), Type::Lst(b)) => a == b,
            (Type::Fun(a, c), Type::Fun(b, d)) => a == b && c == d,
            (Type::Unk(_), _) | (_, Type::Unk(_)) => true,
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
                a.clone()
                    .into_iter()
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
    type IntoIter = <List as IntoIterator>::IntoIter;

    fn into_iter(self) -> Self::IntoIter {
        match self {
            Value::Lst(u) => u.into_iter(),
            _ => unreachable!(),
        }
    }
}

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
