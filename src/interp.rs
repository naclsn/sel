use std::io::{self, Read};
use std::iter;

use crate::parse::{Token, Tree};

pub struct Number(pub Box<dyn FnOnce() -> i32>);
pub struct Bytes(pub Box<dyn FnMut() -> Option<Vec<u8>>>);
pub struct List(pub Box<dyn FnMut() -> Option<Value>>);
pub struct Func(pub Box<dyn FnOnce(Value) -> Value>);

// bytes and list to standard iterators {{{
pub struct BytesIterator(Bytes, Option<<Vec<u8> as IntoIterator>::IntoIter>);

impl Iterator for BytesIterator {
    type Item = u8;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(iter) = &mut self.1 {
            let r = iter.next();
            if r.is_some() {
                return r;
            }
        }
        if let Some(chunk) = self.0 .0() {
            let mut iter = chunk.into_iter();
            let r = iter.next();
            self.1 = Some(iter);
            r
        } else {
            None
        }
    }
}

impl IntoIterator for Bytes {
    type Item = u8;
    type IntoIter = BytesIterator;

    fn into_iter(self) -> Self::IntoIter {
        BytesIterator(self, None)
    }
}

impl Iterator for List {
    type Item = Value;

    fn next(&mut self) -> Option<Self::Item> {
        self.0()
    }
}
// }}}

pub enum Value {
    Number(Number),
    Bytes(Bytes),
    List(List),
    Func(Func),
}

impl Value {
    pub fn number(self) -> Number {
        if let Value::Number(r) = self {
            r
        } else {
            unreachable!()
        }
    }
    pub fn bytes(self) -> Bytes {
        if let Value::Bytes(r) = self {
            r
        } else {
            unreachable!()
        }
    }
    pub fn list(self) -> List {
        if let Value::List(r) = self {
            r
        } else {
            unreachable!()
        }
    }
    pub fn func(self) -> Func {
        if let Value::Func(r) = self {
            r
        } else {
            unreachable!()
        }
    }
}

pub fn lookup_val(name: &str, mut args: impl Iterator<Item = Value>) -> Value {
    match name {
        "input" => {
            let mut stdin = io::stdin();
            Value::Bytes(Bytes(Box::new(move || {
                let mut r = vec![0];
                match stdin.read(&mut r[..]) {
                    Ok(1) => Some(r),
                    _ => None,
                }
            })))
        }

        "split" => {
            let mut uncollected_sep = Some(args.next().unwrap().bytes().into_iter());
            let mut bytes_iter = args.next().unwrap().bytes().into_iter().peekable();
            let mut sep = Vec::<u8>::new();
            Value::List(List(Box::new(move || {
                bytes_iter.peek()?;
                if let Some(a) = uncollected_sep.take() {
                    sep = a.collect();
                }
                let mut v = Some(bytes_iter.by_ref().take_while(|x| *x != sep[0]).collect());
                Some(Value::Bytes(Bytes(Box::new(move || v.take()))))
            })))
        }

        "join" => {
            let mut uncollected_sep = Some(args.next().unwrap().bytes().into_iter());
            let mut list_iter = args.next().unwrap().list().map(Value::bytes).peekable();
            let mut sep = Vec::<u8>::new();
            Value::Bytes(Bytes(Box::new(move || {
                if let Some(ref mut chunks) = list_iter.peek_mut() {
                    let chunk = chunks.0();
                    if chunk.is_some() {
                        return chunk;
                    }
                    drop(list_iter.next());
                    if list_iter.peek().is_some() {
                        if let Some(a) = uncollected_sep.take() {
                            sep = a.collect();
                        }
                        return Some(sep.clone());
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
            Token::Word(w) => lookup_val(w, iter::empty()),
            Token::Bytes(v) => {
                let mut v = Some(v.clone());
                Value::Bytes(Bytes(Box::new(move || v.take())))
            }
            &Token::Number(n) => Value::Number(Number(Box::new(move || n))),
            _ => unreachable!(),
        },

        Tree::List(_) => todo!(),

        Tree::Apply(_, name, args) => lookup_val(name, args.iter().map(interp)),
    }
}
