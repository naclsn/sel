use std::fmt;

#[derive(Debug, Clone)]
pub enum Type {
    Num,
    Str,
    Arr(Box<Type>),
    Any,
    Fun,
}

#[derive(Clone)]
pub struct Fun {
    pub name: &'static str,
    pub params: Vec<Type>,
    pub func: fn(Vec<Value>) -> Value,
    pub args: Vec<Value>,
}

impl Fun {
    pub(crate) fn apply(&mut self, arg: Value) -> Self {
        self.args.push(arg);
        self // YYY: bring the Apply structure back..? (more rust-ish)
    }
}

impl fmt::Debug for Fun {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Fun({} :: {:?})", self.name, self.args)
    }
}

impl PartialEq for Fun {
    fn eq(&self, mate: &Fun) -> bool {
        self.name == mate.name
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Num(i32),
    Str(String),
    Arr(Vec<Value>),
    Fun(Fun),
}
