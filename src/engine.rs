use std::fmt;

#[derive(Debug, Clone)]
pub enum Value {
    Num(f32),
    Str(String),
    Arr(Vec<Value>),
    Fun(Box<Function>),
}

pub trait Apply {
    fn apply(self, arg: Value) -> Value;
}

impl Apply for Value {
    fn apply(self, arg: Value) -> Value {
        match self {
            Value::Fun(mut boxed) => {

                if 1 == boxed.arity {
                    // impl Eval for Box<Function>
                    // fn eval(self) -> Value
                    boxed.args.push(arg);
                    return (boxed.func)(boxed.args);
                }

                let niw = format!("[{} {:?}]", boxed.name, arg);
                boxed.args.push(arg);
                Value::Fun(Box::new(
                    Function {
                        name: niw,
                        arity: boxed.arity-1,
                        args: boxed.args,
                        func: boxed.func,
                    }
                ))

            },
            _ => panic!("value is not a function"),
        }
    }
}

#[derive(Clone)]
pub struct Function {
    pub name: String,
    pub arity: usize,
    pub args: Vec<Value>,
    pub func: fn(Vec<Value>) -> Value,
}

impl fmt::Debug for Function {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Function({:?}, {:?})", self.name, self.args)
    }
}
