use std::{fmt, slice};

#[derive(Debug, Clone)]
pub enum Value {
    Num(f32),
    Str(String),
    Arr(Array),
    Fun(Function),
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Value::Num(n) => write!(f, "{n}"),
            Value::Str(s) => write!(f, "{{{s}}}"),
            Value::Arr(a) =>
                write!(f, "@{{{}}}", a.items
                    .iter()
                    .map(|v| v.to_string())
                    .collect::<Vec<String>>()
                    .join(", ")),
            Value::Fun(g) =>
                write!(f, "{} {}",
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
                        .join(" ")),
        }
    }
}

#[derive(Debug, Clone)]
pub enum Type {
    Num,
    Str,
    Arr(Box<Type>),
    Fun(Box<Type>, Box<Type>),
    Unk(String),
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
            (Type::Arr(a), Type::Arr(b)) => a == b,
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
            Type::Arr(a) => write!(f, "[{a}]"),
            Type::Fun(a, b) => match **a {
                Type::Fun(_, _) => write!(f, "({a}) -> {b}"),
                _ => write!(f, "{a} -> {b}"),
            },
            Type::Unk(n) => write!(f, "{n}"),
        }
    }
}

pub trait Typed {
    fn typed(&self) -> Type;
    fn coerse(self, to: Type) -> Option<Value>;
}

impl Typed for Value {
    fn typed(&self) -> Type {
        match self {
            Value::Num(_) => Type::Num,
            Value::Str(_) => Type::Str,
            Value::Arr(a) => Type::Arr(Box::new(a.has.clone())),
            Value::Fun(f) => Type::Fun(Box::new(f.maps.0.clone()), Box::new(f.maps.1.clone())),
        }
    }

    fn coerse(self, to: Type) -> Option<Value> {
        match to {
            Type::Num =>
                match self {
                    Value::Num(v) => Some(Value::Num(v)),
                    Value::Str(v) => v.parse().ok().map(|r| Value::Num(r)),
                    _ => None,
                },
            Type::Str =>
                match self {
                    Value::Num(v) => Some(Value::Str(v.to_string())),
                    Value::Str(v) => Some(Value::Str(v)),
                    // Value::Arr(v) => Some(Value::Str(v.to_string())),
                    _ => None,
                },
            Type::Arr(ref t) if Type::Num == **t =>
                match self {
                    Value::Str(v) => Some(Value::Arr(Array {
                        has: to,
                        items: v
                            .chars()
                            .map(|c| Value::Num((c as u32) as f32))
                            .collect(),
                    })),
                    Value::Arr(v) => {
                        let may = v.items
                            .into_iter()
                            .map(|i| i.coerse(Type::Num))
                            .collect::<Option<Vec<Value>>>();
                        match may {
                            Some(items) => Some(Value::Arr(Array { has: to, items })),
                            None => None,
                        }
                    },
                    _ => None,
                },
            Type::Arr(ref t) if Type::Str == **t =>
                match self {
                    Value::Str(v) => Some(Value::Arr(Array {
                        has: to,
                        items: v
                            .chars()
                            .map(|c| Value::Str(c.to_string()))
                            .collect(),
                    })),
                    Value::Arr(v) => {
                        let may = v.items
                            .into_iter()
                            .map(|i| i.coerse(Type::Str))
                            .collect::<Option<Vec<Value>>>();
                        match may {
                            Some(items) => Some(Value::Arr(Array { has: to, items })),
                            None => None,
                        }
                    },
                    _ => None,
                },
            _ => None,
        }
    }
}

pub trait Apply {
    fn apply(self, arg: Value) -> Value;
}

impl Apply for Value {
    fn apply(self, arg: Value) -> Value {
        let crap = self.clone().typed();
        match self {

            Value::Fun(mut fun) => {
                if fun.maps.0 != arg.typed() {
                    // TODO: attempt implicit type conversion here with arg.coerse().expect(..)
                    println!("wrong type of argument for function:");
                    println!("  applying argument: {}", arg.typed());
                    println!("        to function: {}", crap);
                    panic!("type error");
                }

                fun.args.push(arg);
                (fun.func)(fun)
            },

            other => {
                println!("value is not a function:");
                println!("  applying argument: {}", arg.typed());
                println!("         to a value: {}", other.typed());
                panic!("type error");
            },

        }
    }
}

#[derive(Debug, Clone)]
pub struct Array {
    pub has: Type,
    pub items: Vec<Value>,
}

impl<'a> IntoIterator for &'a Array {
    type Item = &'a Value;
    type IntoIter = slice::Iter<'a, Value>;

    fn into_iter(self) -> Self::IntoIter { self.items.iter() }
}

#[derive(Clone)]
pub struct Function {
    pub name: String,
    pub maps: (Type, Type),
    pub args: Vec<Value>,
    pub func: fn(Function) -> Value,
}

impl fmt::Debug for Function {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Function({:?}, {:?}, {:?}, {:?})", self.name, self.maps, self.args, self.func)
    }
}
