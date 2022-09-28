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

#[derive(Clone)]
pub struct Fit {
    pub name: &'static str,
    pub arity: u8, // YYY: + type hints?
    pub func: fn(Vec<Value>) -> Value,
}

#[derive(Debug, Clone)]
pub enum Value {
    Num(i32),
    Str(String),
    Arr(Vec<Value>),
}

pub struct Apply {
    pub fit: Fit,
    pub args: Vec<Value>,
}

