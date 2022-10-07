use std::fmt;

// NOTE: `Debug` is kept for rust-level debugging,
//       `Display` is used for sel-level debugging
//       as such, the actual output for a type of
//       value is through Value::to_text

#[derive(Debug, Clone)]
pub enum Type {
    Num,
    Str,
    Arr(Box<Type>),
    Fun(Box<Type>, Box<Type>),
    Unk(String),
}

pub trait Typed {
    fn typed(&self) -> Type;
    fn coerse(self, to: Type) -> Option<Value>;
}

#[derive(Debug, Clone)]
pub enum Value {
    Num(Number),
    Str(String),
    Arr(Array),
    Fun(Function),
}

impl Value {
    pub fn as_text(&self) -> String {
        match self {
            Value::Num(n) => n.to_string(),
            Value::Str(s) => s.to_string(),
            Value::Arr(a) => match &a.has {
                Type::Num | Type::Str => a
                    .items
                    .iter()
                    .map(Value::as_text)
                    .collect::<Vec<String>>()
                    .join(" "),
                Type::Arr(t) => match **t {
                    Type::Num | Type::Str => a
                        .items
                        .iter()
                        .map(Value::as_text)
                        .collect::<Vec<String>>()
                        .join("\n"),
                    _ => panic!(
                        "no acceptable text conversion for the last return type (deep array)"
                    ),
                },
                _ => panic!(
                    "no acceptable text conversion for the last return type (array of functions)"
                ),
            },
            _ => panic!("no acceptable text conversion for the last return type (function)"),
        }
    }
}

pub type Number = f32;

#[derive(Debug, Clone)]
pub struct Array {
    pub has: Type,
    pub items: Vec<Value>,
}

#[derive(Clone)]
pub struct Function {
    pub name: String,
    pub maps: (Type, Type),
    pub args: Vec<Value>,
    pub func: fn(Function) -> Value,
}

impl Apply for Function {
    fn apply(mut self, arg: Value) -> Value {
        let given_type = arg.typed();

        // YYY: can have a 'no auto coerse' flag for functions like `tonum` and `tostr`
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

pub trait Apply {
    fn apply(self, arg: Value) -> Value;
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
            Type::Arr(ref t) if Type::Num == **t => match self {
                Value::Str(v) => Some(Value::Arr(Array {
                    has: Type::Num,
                    items: v.chars().map(|c| Value::Num((c as u32) as Number)).collect(),
                })),
                Value::Arr(v) => {
                    let may = v
                        .items
                        .into_iter()
                        .map(|i| i.coerse(Type::Num))
                        .collect::<Option<Vec<Value>>>();
                    match may {
                        Some(items) => Some(Value::Arr(Array { has: *t.clone(), items })),
                        None => None,
                    }
                }
                _ => None,
            },
            Type::Arr(ref t) if Type::Str == **t => match self {
                Value::Str(v) => Some(Value::Arr(Array {
                    has: Type::Str,
                    items: v.chars().map(|c| Value::Str(c.to_string())).collect(),
                })),
                Value::Arr(v) => {
                    let may = v
                        .items
                        .into_iter()
                        .map(|i| i.coerse(Type::Str))
                        .collect::<Option<Vec<Value>>>();
                    match may {
                        Some(items) => Some(Value::Arr(Array { has: *t.clone(), items })),
                        None => None,
                    }
                }
                _ => None,
            },
            _ => None,
        }
    }
}

impl Apply for Value {
    fn apply(self, arg: Value) -> Value {
        match self {
            Value::Fun(fun) => fun.apply(arg),

            other => {
                println!("value is not a function:");
                println!("  applying argument: {}", arg.typed());
                println!("         to a value: {}", other.typed());
                panic!("type error");
            }
        }
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

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Value::Num(n) => write!(f, "{n}"),
            Value::Str(s) => write!(f, "{{{s}}}"),
            Value::Arr(a) => write!(
                f,
                "@{{{}}}",
                a.items
                    .iter()
                    .map(|v| match v {
                        Value::Str(s) => s.clone(),
                        Value::Arr(_) => v.to_string()[1..].to_string(),
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

// TODO: all of them and a bit more (more macro anyone?)

impl From<Number> for Value {
    fn from(n: Number) -> Self {
        Value::Num(n)
    }
}
impl From<Value> for Number {
    fn from(v: Value) -> Self {
        match v {
            Value::Num(n) => n,
            _ => unreachable!(),
        }
    }
}

impl From<String> for Value {
    fn from(_: String) -> Self {
        todo!()
    }
}
impl From<Value> for String {
    fn from(v: Value) -> Self {
        match v {
            Value::Str(s) => s,
            _ => unreachable!(),
        }
    }
}

impl From<&str> for Value {
    fn from(v: &str) -> Self {
        Value::Str(v.to_string())
    }
}
impl From<Value> for &str {
    fn from(_: Value) -> Self {
        todo!()
    }
}

// impl FromIterator<Value> for Value {
//     fn from_iter<T: IntoIterator<Item = Value>>(_iter: T) -> Self {
//         todo!() // XXX: still will not be able to infer the contained type!
//     }
// }

impl<'a> FromIterator<&'a str> for Value {
    fn from_iter<T: IntoIterator<Item = &'a str>>(iter: T) -> Self {
        Value::Arr(Array {
            has: Type::Str,
            items: iter.into_iter().map(|s| Value::from(s)).collect(),
        })
    }
}

impl From<Array> for Value {
    fn from(a: Array) -> Self {
        Value::Arr(a)
    }
}
impl From<Value> for Array {
    fn from(v: Value) -> Self {
        match v {
            Value::Arr(a) => a,
            _ => unreachable!(),
        }
    }
}

// impl From<Vec<Value>> for Value {
//     fn from(_: Vec<Value>) -> Self {
//         todo!() // XXX: still will not be able to infer the contained type!
//     }
// }
impl From<Value> for Vec<Value> {
    fn from(v: Value) -> Self {
        match v {
            Value::Arr(a) => a.items,
            _ => unreachable!(),
        }
    }
}

impl From<Vec<Number>> for Value {
    fn from(_: Vec<Number>) -> Self {
        todo!()
    }
}
impl From<Value> for Vec<Number> {
    fn from(v: Value) -> Self {
        match v {
            Value::Arr(a) => a.items.into_iter().map(|it| it.into()).collect(),
            _ => unreachable!(),
        }
    }
}

impl From<Vec<String>> for Value {
    fn from(_: Vec<String>) -> Self {
        todo!()
    }
}
impl From<Value> for Vec<String> {
    fn from(v: Value) -> Self {
        match v {
            Value::Arr(a) => a.items.into_iter().map(|it| it.into()).collect(),
            _ => unreachable!(),
        }
    }
}

impl From<Vec<&str>> for Value {
    fn from(_: Vec<&str>) -> Self {
        todo!()
    }
}
impl From<Value> for Vec<&str> {
    fn from(v: Value) -> Self {
        match v {
            Value::Arr(a) => a.items.into_iter().map(|it| it.into()).collect(),
            _ => unreachable!(),
        }
    }
}

impl<const L: usize> From<[Number; L]> for Value {
    fn from(_: [Number; L]) -> Self {
        todo!()
    }
}

impl<const L: usize> From<[String; L]> for Value {
    fn from(_: [String; L]) -> Self {
        todo!()
    }
}

impl From<Function> for Value {
    fn from(_: Function) -> Self {
        todo!()
    }
}
impl From<Value> for Function {
    fn from(v: Value) -> Self {
        match v {
            Value::Fun(f) => f,
            _ => unreachable!(),
        }
    }
}
