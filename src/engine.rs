use std::{fmt, slice};

#[derive(Debug, Clone)]
pub enum Value {
    Num(f32),
    Str(String),
    Arr(Array),
    Fun(Function),
}

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Num,
    Str,
    Arr(Box<Type>),
    Fun(Box<Type>, Box<Type>),
}

pub trait Typed {
    fn typed(self) -> Type;
    fn coerse(self, to: Type) -> Option<Value>;
}

impl Typed for Value {
    fn typed(self) -> Type {
        match self {
            Value::Num(_) => Type::Num,
            Value::Str(_) => Type::Str,
            Value::Arr(a) => Type::Arr(Box::new(a.types)),
            Value::Fun(f) => Type::Fun(Box::new(f.types.0), Box::new(f.types.1)),
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
            Type::Arr(t) if Type::Num == *t =>
                match self {
                    Value::Str(v) => Some(Value::Arr(
                        Array {
                            types: to,
                            items: v
                                .chars()
                                .map(|c| Value::Num((c as u32) as f32))
                                .collect(),
                        }
                    )),
                    Value::Arr(v) => v.items
                        .into_iter()
                        .map(|i| i.coerse(Type::Num))
                        .collect::<Option<Vec<Value>>>()
                        .map(|r| Value::Arr(
                            Array {
                                types: to,
                                items: r,
                            }
                        )),
                    _ => None,
                },
            Type::Arr(t) if Type::Str == *t =>
                match self {
                    Value::Str(v) => Some(Value::Arr(
                        Array {
                            types: to,
                            items: v
                                .chars()
                                .map(|c| Value::Str(c.to_string()))
                                .collect(),
                        }
                    )),
                    Value::Arr(v) => v.items
                        .into_iter()
                        .map(|i| i.coerse(Type::Str))
                        .collect::<Option<Vec<Value>>>()
                        .map(|r| Value::Arr(
                            Array {
                                types: to,
                                items: r,
                            }
                        )),
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
        match self {

            Value::Fun(mut boxed) => {
                boxed.args.push(arg);

                if 1 == boxed.arity {
                    return (boxed.func)(boxed.args);
                }

                let (oldin, oldout) = boxed.types;
                let argt = arg.typed();
                // TODO: update type with new given argument
                /*
                    // YYY: example with `map`

                    if let Value::Fun(f) = arg {           // kind check
                        let (subin, subout) = f.types;     // sub-types extraction

                        // now we know that:               // associate with variables
                        //    a <- subin
                        //    b <- subout
                        // hence the new type

                        let (newin, newout) =              // construct result type
                            ( Type::Arr(subin)
                            , Type::Arr(subout)
                        );

                    } else { panic!("type error"); }
                */
                let (newin, newout) = boxed.types;

                Value::Fun(
                    Function {
                        arity: boxed.arity-1,
                        types: (newin, newout),
                        args: boxed.args,
                        func: boxed.func,
                    }
                )

            },

            _ => panic!("value is not a function"),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Array {
    pub types: Type,
    pub items: Vec<Value>,
}

// impl Array {
//     pub fn new(types: Type, items: Vec<Value>) -> Self { Self { types, items } }
// }

impl<'a> IntoIterator for &'a Array {
    type Item = &'a Value;
    type IntoIter = slice::Iter<'a, Value>;

    fn into_iter(self) -> Self::IntoIter {
        self.items.iter()
    }
}

// impl FromIterator<Value> for Array {
//     fn from_iter<T: IntoIterator<Item = Value>>(iter: T) -> Self {
//         Array {
//             types: Type::Str, // XXX
//             items: <Vec<Value>>::from_iter(iter),
//         }
//     }
// }

#[derive(Clone)]
pub struct Function {
    pub arity: usize,
    pub types: (Type, Type),
    pub args: Vec<Value>,
    pub func: fn(Vec<Value>) -> Value,
}

impl fmt::Debug for Function {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Function({:?}, {:?})", "self.name", self.args) // TODO: association with Token
    }
}
