use std::io::{self, Read};

use crate::parse::Error;

pub trait NumberEval { fn number_eval(self) -> i32; }
pub trait BytesStream { fn bytes_stream(&mut self) -> Option<Vec<u8>>; }
pub trait ListNext { fn list_next(&mut self) -> Option<Value>; }
pub trait FuncCall { fn func_call(self) -> Value; }

// bytes and list to standard iterators {{{
pub struct BytesStreamIterator(
    Box<dyn BytesStream>,
    Option<<Vec<u8> as IntoIterator>::IntoIter>,
);

impl Iterator for BytesStreamIterator {
    type Item = u8;

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(iter) = &mut self.1 {
            iter.next()
        } else if let Some(chunk) = self.0.bytes_stream() {
            let mut iter = chunk.into_iter();
            let r = iter.next();
            self.1 = Some(iter);
            r
        } else {
            None
        }
    }
}

impl IntoIterator for Box<dyn BytesStream> {
    type Item = u8;
    type IntoIter = BytesStreamIterator;

    fn into_iter(self) -> Self::IntoIter {
        BytesStreamIterator(self, Some(vec![].into_iter()))
    }
}

impl Iterator for Box<dyn ListNext> {
    type Item = Value;

    fn next(&mut self) -> Option<Self::Item> {
        self.list_next()
    }
}
// }}}

pub type Number = Box<dyn NumberEval>;
pub type Bytes = Box<dyn BytesStream>;
pub type List = Box<dyn ListNext>;
pub type Func = Box<dyn FuncCall>;

pub enum Value {
    Number(Number),
    Bytes(Bytes),
    List(List),
    Func(Func),
}

impl Value {
    pub fn number(self) -> Number {
        if let Value::Number(r) = self { r } else { unreachable!() }
    }
    pub fn bytes(self) -> Bytes {
        if let Value::Bytes(r) = self { r } else { unreachable!() }
    }
    pub fn list(self) -> List {
        if let Value::List(r) = self { r } else { unreachable!() }
    }
    pub fn func(self) -> Func {
        if let Value::Func(r) = self { r } else { unreachable!() }
    }
}

pub fn lookup_val(name: &str, mut args: Vec<Value>) -> Result<Value, Error> {
    match name {
        "input" => Ok(Value::Bytes(Box::new(Input(io::stdin())))),
        "split" => {
            let b = args.pop().unwrap().bytes();
            let a = args.pop().unwrap().bytes();
            Ok(Value::List(Box::new(Split(Some(a), Vec::new(), b.into_iter()))))
        },
        _ => Err(Error::UnknownName(name.to_string())),
    }
}

// value implementations {{{
struct Input(io::Stdin);
impl BytesStream for Input {
    fn bytes_stream(&mut self) -> Option<Vec<u8>> {
        let mut c = vec![0u8];
        self.0.read(&mut c[..]).ok().and_then(|k| if 0 == k { None } else { Some(c) })
    }
}

struct Split(Option<Bytes>, Vec<u8>, BytesStreamIterator);
struct SplitChunk(Vec<u8>);
impl ListNext for Split {
    fn list_next(&mut self) -> Option<Value> {
        if let Some(b) = self.0.take() {
            self.1 = b.into_iter().collect();
        }
        let chunk: Vec<u8> =;
            self.2.by_ref().take_while(|&x| self.1[0] != x).collect();
        Some(SplitChunk)
    }
}
// }}}
