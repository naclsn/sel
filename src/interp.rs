use std::io::{self, Read, Write};
use std::iter;

use crate::parse::{Tree, TreeLeaf};

pub type Number = Box<dyn FnOnce() -> i32>;
pub type Bytes = Box<dyn Iterator<Item = u8>>;
pub type List = Box<dyn Iterator<Item = Value>>;
pub type Func = Box<dyn FnOnce(Value) -> Value>;

pub enum Value {
    Number(Number),
    Bytes(Bytes),
    List(List),
    Func(Func),
}

impl Value {
    fn number(self) -> Number {
        if let Value::Number(r) = self {
            r
        } else {
            unreachable!()
        }
    }
    fn bytes(self) -> Bytes {
        if let Value::Bytes(r) = self {
            r
        } else {
            unreachable!()
        }
    }
    fn list(self) -> List {
        if let Value::List(r) = self {
            r
        } else {
            unreachable!()
        }
    }
    fn func(self) -> Func {
        if let Value::Func(r) = self {
            r
        } else {
            unreachable!()
        }
    }
}

fn lookup_val(name: &str, mut args: impl Iterator<Item = Value>) -> Value {
    match name {
        "input" => {
            let mut stdin = io::stdin();
            Value::Bytes(Box::new(iter::from_fn(move || {
                let mut r = [0u8; 1];
                match stdin.read(&mut r) {
                    Ok(1) => Some(r[0]),
                    _ => None,
                }
            })))
        }

        "split" => {
            let mut uncollected_sep = Some(args.next().unwrap().bytes());
            let mut bytes_iter = args.next().unwrap().bytes().peekable();
            let mut sep = Vec::<u8>::new();
            Value::List(Box::new(iter::from_fn(move || {
                bytes_iter.peek()?;
                if let Some(a) = uncollected_sep.take() {
                    sep = a.collect();
                }
                let v: Vec<u8> = bytes_iter.by_ref().take_while(|x| *x != sep[0]).collect();
                Some(Value::Bytes(Box::new(v.into_iter())))
            })))
        }

        "add" => {
            let a = args.next().unwrap().number();
            let b = args.next().unwrap().number();
            Value::Number(Box::new(|| a() + b()))
        }

        //"map" => {
        //    let f = args.next().unwrap().func();
        //    let mut l = args.next().unwrap().list();
        //    Value::List(Box::new(iter::from_fn(move || l.next().map(f))))
        //}

        "join" => {
            let mut uncollected_sep = Some(args.next().unwrap().bytes().into_iter());
            let mut list_iter = args.next().unwrap().list().map(Value::bytes).peekable();
            let mut sep = Vec::<u8>::new();
            Value::Bytes(Box::new(iter::from_fn(move || {
                if let Some(ref mut bytes) = list_iter.peek_mut() {
                    let b = bytes.next();
                    if b.is_some() {
                        return b;
                    }
                    drop(list_iter.next());
                    if list_iter.peek().is_some() {
                        if let Some(a) = uncollected_sep.take() {
                            sep = a.collect();
                        }
                        return Some(sep[0]);
                    }
                }
                None
            })))
        }

        _ => unreachable!(),
    }
}

pub fn interp(tree: &Tree) -> Value {
    match tree {
        Tree::Atom(atom) => match atom {
            TreeLeaf::Word(w) => lookup_val(w, iter::empty()),
            TreeLeaf::Bytes(v) => Value::Bytes(Box::new(v.clone().into_iter())),
            &TreeLeaf::Number(n) => Value::Number(Box::new(move || n)),
        },

        Tree::List(_) => todo!(),

        Tree::Apply(_, name, args) => lookup_val(name, args.iter().map(interp)),
    }
}

pub fn run_print(val: Value) {
    match val {
        Value::Number(n) => println!("{}", n()),
        Value::Bytes(b) => {
            let mut stdout = io::stdout();
            for ch in b {
                stdout.write_all(&[ch]).unwrap();
            }
        }
        Value::List(l) => {
            for it in l {
                run_print(it);
                println!();
            }
        }
        Value::Func(_f) => todo!(),
    }
}
