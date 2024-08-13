use std::io::{self, Read, Write};
use std::iter;
use std::rc::Rc;

use crate::parse::{Tree, TreeKind, COMPOSE_OP_FUNC_NAME};

// runtime concrete value and macro to make curried instances {{{
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
    fn number(self) -> Number {
        if let Value::Number(r, _) = self {
            r
        } else {
            unreachable!("runtime type mismatchs should not be possible")
        }
    }
    fn bytes(self) -> Bytes {
        if let Value::Bytes(r, _) = self {
            r
        } else {
            unreachable!("runtime type mismatchs should not be possible")
        }
    }
    fn list(self) -> List {
        if let Value::List(r, _) = self {
            r
        } else {
            unreachable!("runtime type mismatchs should not be possible")
        }
    }
    fn func(self) -> Func {
        if let Value::Func(r, _) = self {
            r
        } else {
            unreachable!("runtime type mismatchs should not be possible")
        }
    }
}

macro_rules! curried_value {
    (|| -> $what:ident $inside:expr) => {
        curried_value!(@reverse [] [], $what, $inside)
    };
    (|$($args:ident),*| -> $what:ident $inside:expr) => {
        curried_value!(@reverse [$($args),*] [], $what, $inside)
    };

    (@reverse [] $args:tt, $what:ident, $inside:expr) => {
        curried_value!(@build $args, $what, $inside)
    };
    (@reverse [$h:ident$(, $t:ident)*] [$($r:ident),*], $what:ident, $inside:expr) => {
        curried_value!(@reverse [$($t),*] [$h$(, $r)*], $what, $inside)
    };

    (@build [], $what:ident, $inside:expr) => {
        {
            let clone = $inside;
            Value::$what(Box::new(clone.clone()), Rc::new(move || Box::new(clone.clone())))
        }
    };
    (@build [$h:ident$(, $t:ident)*], $what:ident, $inside:expr) => {
        curried_value!(
            @build
            [$($t),*],
            Func,
            |$h: Value| {
                let clone = move || {
                    let $h = $h.clone();
                    $(let $t = $t.clone();)*
                    $inside
                };
                Value::$what(Box::new(clone()), Rc::new(move || Box::new(clone())))
            }
        )
    };
}
// }}}

// lookup and make {{{
fn lookup_val(name: &str, mut args: impl Iterator<Item = Value>) -> Value {
    if COMPOSE_OP_FUNC_NAME == name {
        let f = args.next().unwrap();
        let g = args.next().unwrap();
        let does = |v| g.func()(f.func()(v));
        return Value::Func(
            Box::new(does.clone()),
            Rc::new(move || Box::new(does.clone())),
        );
    }

    let apply_args = move |v: Value| args.fold(v, |acc, cur| acc.func()(cur));

    match name {
        "add" => apply_args(curried_value!(|a, b| -> Number || a.number()() + b.number()())),

        "const" => apply_args(curried_value!(|a| -> Func |_| a)),

        "input" => apply_args(curried_value!(|| -> Bytes {
            iter::from_fn(move || {
                // XXX: this will be incorrect very easily but whever for now
                // (ex: `repeat input` just.. cannot be)
                let mut stdin = io::stdin();
                let mut r = [0u8; 1];
                match stdin.read(&mut r) {
                    Ok(1) => Some(r[0]),
                    _ => None,
                }
            })
        })),

        "join" => apply_args(curried_value!(|sep, lst| -> Bytes {
            let sep = sep.bytes().collect::<Vec<_>>();
            let mut lst = lst.list().map(Value::bytes).peekable();
            let mut now_sep = 0;
            iter::from_fn(move || {
                if 0 != now_sep {
                    if now_sep < sep.len() {
                        now_sep += 1;
                        return Some(sep[now_sep - 1]);
                    };
                    now_sep = 0;
                }
                if let Some(bytes) = lst.peek_mut() {
                    let b = bytes.next();
                    if b.is_some() {
                        return b;
                    }
                    drop(lst.next());
                    if lst.peek().is_some() {
                        now_sep = 1;
                        return Some(sep[0]);
                    }
                }
                None
            })
        })),

        "len" => apply_args(curried_value!(|list| -> Number || list.list().count() as f64)),

        "ln" => apply_args(curried_value!(|s| -> Bytes s.bytes().chain(iter::once(b'\n')))),

        "map" => {
            apply_args(curried_value!(|f, l| -> List l.list().map(move |i| f.clone().func()(i))))
        }

        "repeat" => apply_args(curried_value!(|val| -> List iter::repeat(val))),

        "split" => apply_args(curried_value!(|sep, s| -> List {
            let sep = sep.bytes().collect::<Vec<_>>();
            let mut s = s.bytes().peekable();
            let mut updog = Vec::<u8>::new();
            // XXX: not as lazy as could be (but I got lazy instead)
            // FIXME: also it skips the last empty split if s exactly ends with sep
            iter::from_fn(move || {
                s.peek()?;
                for b in s.by_ref() {
                    updog.push(b);
                    if updog.ends_with(&sep) {
                        updog.truncate(updog.len() - sep.len());
                        break;
                    }
                }
                let v = std::mem::take(&mut updog);
                let vv = v.clone();
                Some(Value::Bytes(
                    Box::new(v.into_iter()),
                    Rc::new(move || Box::new(vv.clone().into_iter())),
                ))
            })
        })),

        "tonum" => apply_args(curried_value!(|s| -> Number || {
            let mut r = 0;
            let mut s = s.bytes();
            for n in s.by_ref() {
                if n.is_ascii_digit() {
                    r = 10 * r + (n - b'0') as usize;
                } else if b'.' == n {
                    let mut d = 0;
                    let mut w = 0;
                    for n in s.take_while(u8::is_ascii_digit) {
                        d = 10 * d + (n - b'0') as usize;
                        w *= 10;
                    }
                    return r as f64 + (d as f64 / w as f64);
                } else {
                    break;
                }
            }
            r as f64
        })),

        "tostr" => apply_args(
            curried_value!(|n| -> Bytes n.number()().to_string().into_bytes().into_iter()),
        ),

        "zipwith" => apply_args(
            curried_value!(|f, la, lb| -> List la.list().zip(lb.list()).map(move |(a, b)| f.clone().func()(a).func()(b))),
        ),

        _ => unreachable!(
            "well as long as this list is kept up to date with builtin::NAME that is..."
        ),
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

        TreeKind::List(_, items) => {
            let v = items.iter().map(interp).collect::<Vec<_>>();
            let vv = v.clone();
            Value::List(
                Box::new(v.into_iter()),
                Rc::new(move || Box::new(vv.clone().into_iter())),
            )
        }

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
