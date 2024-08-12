use std::cell::RefCell;
use std::io::{self, Read, Write};
use std::iter;
use std::mem;
use std::ptr;
use std::rc::Rc;

use crate::parse::{Tree, TreeKind, COMPOSE_OP_FUNC_NAME};

// runtime concrete value {{{
pub type Number = Box<dyn FnOnce() -> f64>;
pub type Bytes = Box<dyn Iterator<Item = u8>>;
pub type List = Box<dyn Iterator<Item = Value>>;
pub type Func = Box<dyn FnOnce(Value) -> Value>;

type ValueClone<T> = Rc<dyn Fn() -> T>;

pub enum Value {
    Number(Number, ValueClone<Number>),
    Bytes(Bytes, ValueClone<Bytes>),
    List(List, ValueClone<List>),
    Func(Func, ValueClone<Func>),
}

impl Clone for Value {
    fn clone(&self) -> Self {
        match self {
            Value::Number(_, clone) => {
                let niw = clone();
                Value::Number(niw, clone.clone())
            }
            Value::Bytes(_, clone) => {
                let niw = clone();
                Value::Bytes(niw, clone.clone())
            }
            Value::List(_, clone) => {
                let niw = clone();
                Value::List(niw, clone.clone())
            }
            Value::Func(_, clone) => {
                let niw = clone();
                Value::Func(niw, clone.clone())
            }
        }
    }
}

impl Value {
    fn number(self) -> (Number, ValueClone<Number>) {
        if let Value::Number(r, c) = self {
            (r, c)
        } else {
            unreachable!()
        }
    }
    fn bytes(self) -> (Bytes, ValueClone<Bytes>) {
        if let Value::Bytes(r, c) = self {
            (r, c)
        } else {
            unreachable!()
        }
    }
    fn list(self) -> (List, ValueClone<List>) {
        if let Value::List(r, c) = self {
            (r, c)
        } else {
            unreachable!()
        }
    }
    fn func(self) -> (Func, ValueClone<Func>) {
        if let Value::Func(r, c) = self {
            (r, c)
        } else {
            unreachable!()
        }
    }
}
// }}}

// Bival<A, B> (essentially a lazy value then its result cached) {{{
enum Bival<A, B> {
    Init(A),
    Fini(B),
}

impl<A, B> Bival<A, B> {
    fn new(init: A) -> Bival<A, B> {
        Bival::Init(init)
    }

    fn map(&mut self, f: impl FnOnce(A) -> B) -> &mut B {
        match self {
            Bival::Init(a) => {
                let niw = unsafe { Bival::Fini(f(ptr::read(a))) };
                let Bival::Init(old) = mem::replace(self, niw) else {
                    unreachable!()
                };
                _ = mem::MaybeUninit::new(old);
                let Bival::Fini(b) = self else { unreachable!() };
                b
            }
            Bival::Fini(b) => b,
        }
    }
}
// }}}

// lookup and make {{{
fn lookup_val(name: &str, mut args: impl Iterator<Item = Value>) -> Value {
    match name {
        COMPOSE_OP_FUNC_NAME => {
            let f = args.next().unwrap().func().0;
            let g = args.next().unwrap().func().0;
            Value::Func(Box::new(move |v| g(f(v))), Rc::new(|| todo!()))
        }

        "input" => {
            let mut stdin = io::stdin();
            Value::Bytes(
                Box::new(iter::from_fn(move || {
                    let mut r = [0u8; 1];
                    match stdin.read(&mut r) {
                        Ok(1) => Some(r[0]),
                        _ => None,
                    }
                })),
                Rc::new(|| todo!()),
            )
        }

        "ln" => Value::Bytes(
            Box::new(args.next().unwrap().bytes().0.chain(iter::once(b'\n'))),
            Rc::new(|| todo!()),
        ),

        "split" => {
            let mut uncollected_sep = Some(args.next().unwrap().bytes().0);
            let mut bytes_iter = args.next().unwrap().bytes().0.peekable();
            let mut sep = Vec::<u8>::new();
            Value::List(
                Box::new(iter::from_fn(move || {
                    bytes_iter.peek()?;
                    if let Some(a) = uncollected_sep.take() {
                        sep = a.collect();
                    }
                    let v: Vec<u8> = bytes_iter.by_ref().take_while(|x| *x != sep[0]).collect();
                    Some(Value::Bytes(Box::new(v.into_iter()), Rc::new(|| todo!())))
                })),
                Rc::new(|| todo!()),
            )
        }

        "add" => {
            let a = args.next().unwrap().number().0;
            if let Some(b) = args.next() {
                let b = b.number().0;
                Value::Number(Box::new(move || a() + b()), Rc::new(|| todo!()))
            } else {
                let a = Rc::new(RefCell::new(Bival::new(a)));
                let f = move |b: Value| {
                    let b = b.number().0;
                    Value::Number(
                        Box::new(move || *a.borrow_mut().map(|a| a()) + b()),
                        Rc::new(|| todo!()),
                    )
                };
                Value::Func(Box::new(f.clone()), Rc::new(move || Box::new(f.clone())))
            }
        }

        "map" => {
            let (mut f, f_clone) = args.next().unwrap().func();
            let mut l = args.next().unwrap().list().0;
            Value::List(
                Box::new(iter::from_fn(move || {
                    l.next().map(mem::replace(&mut f, f_clone()))
                })),
                Rc::new(|| todo!()),
            )
        }

        "zipwith" => {
            todo!()
        }

        "tonum" => {
            let s = args.next().unwrap().bytes().0;
            Value::Number(
                Box::new(move || {
                    let mut r = 0.0;
                    for n in s {
                        if n.is_ascii_digit() {
                            r = 10.0 * r + (n - b'0') as f64;
                        } else {
                            break;
                        }
                    }
                    r
                }),
                Rc::new(|| todo!()),
            )
        }

        "tostr" => {
            let mut n = Bival::new(args.next().unwrap().number().0);
            Value::Bytes(
                Box::new(iter::from_fn(move || {
                    n.map(|n| n().to_string().into_bytes().into_iter()).next()
                })),
                Rc::new(|| todo!()),
            )
        }

        "join" => {
            let mut uncollected_sep = Some(args.next().unwrap().bytes().0);
            let mut list_iter = args.next().unwrap().list().0.map(Value::bytes).peekable();
            let mut sep = Vec::<u8>::new();
            Value::Bytes(
                Box::new(iter::from_fn(move || {
                    if let Some((ref mut bytes, _)) = list_iter.peek_mut() {
                        let b = bytes.next();
                        if b.is_some() {
                            return b;
                        }
                        list_iter.next();
                        if list_iter.peek().is_some() {
                            if let Some(a) = uncollected_sep.take() {
                                sep = a.collect();
                            }
                            return Some(sep[0]);
                        }
                    }
                    None
                })),
                Rc::new(|| todo!()),
            )
        }

        "len" => {
            let list = args.next().unwrap().list().0;
            Value::Number(Box::new(move || list.count() as f64), Rc::new(|| todo!()))
        }

        "repeat" => {
            let val = args.next().unwrap();
            Value::List(Box::new(iter::repeat(val)), Rc::new(|| todo!()))
        }

        _ => unreachable!("well as long as this list is kept up to date with builtin::NAME that is..."),
    }
}

pub fn interp(tree: &Tree) -> Value {
    match &tree.1 {
        TreeKind::Bytes(v) => {
            let vv = v.clone();
            Value::Bytes(
                Box::new(v.clone().into_iter()),
                Rc::new(move || Box::new(vv.clone().into_iter())),
            )
        }

        &TreeKind::Number(n) => {
            Value::Number(Box::new(move || n), Rc::new(move || Box::new(move || n)))
        }

        TreeKind::List(_, items) => Value::List(
            Box::new(items.iter().map(interp).collect::<Vec<_>>().into_iter()),
            Rc::new(|| todo!()),
        ),

        TreeKind::Apply(_, name, args) => lookup_val(name, args.iter().map(interp)),
    }
}
// }}}

pub fn run_print(val: Value) {
    match val {
        Value::Number(n, _) => println!("{}", n()),
        Value::Bytes(b, _) => {
            let mut stdout = io::stdout();
            for ch in b {
                stdout.write_all(&[ch]).unwrap();
            }
        }
        Value::List(l, _) => {
            for it in l {
                run_print(it);
            }
        }
        Value::Func(_f, _) => panic!("run_print on a function value"),
    }
}
