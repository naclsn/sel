// impl Fit {
//     pub(crate) fn new(name: &'static str, func: fn(Item) -> Item) -> Self { Self { name, func } }
// }

// impl Default for Fit {
//     fn default() -> Fit { Fit { name: "undefined", arry: 0, args: [].to_vec(), func: |_| Token::Num(0) } }
// }

// impl fmt::Debug for Fit {
//     fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
//         write!(f, "Fit({})", self.name)
//     }
// }

// pub enum Type<'a> {
//     Num,
//     Str,
//     Arr(&'a Type<'a>),
//     Any,
// }

#[derive(Clone)]
pub enum Type {
    Num,
    Str,
    Arr(Box<Type>),
    Any,
}

#[derive(Clone)]
pub struct Fit {
    pub name: &'static str,
    pub args: Vec<Type>,
    pub func: fn(Vec<Value>) -> Value,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Num(i32),
    Str(String),
    Arr(Vec<Value>),
}

pub struct Apply {
    pub base: Fit,
    pub args: Vec<Value>,
}

