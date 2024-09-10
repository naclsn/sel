use std::collections::HashMap;
use std::io::{self, Read, Result as IoResult, Stdin, Write};
use std::iter;
use std::mem;
use std::rc::Rc;
use std::sync::{OnceLock, RwLock};

use crate::parse::{Applicable, Pattern, Tree, TreeKind};
use crate::scope::{Global, ScopeItem};

pub fn run_write(tree: &Tree, global: &Global, w: &mut impl Write) -> IoResult<()> {
    run_impl(interp_impl(tree, global, &HashMap::new()), w)
}

// runtime concrete value and macro to make curried instances {{{
pub type Number = Box<dyn FnOnce() -> f64>;
pub type Bytes = Box<dyn Iterator<Item = u8>>;
pub type List = Box<dyn Iterator<Item = Value>>;
pub type Func = Box<dyn FnOnce(Value) -> Value>;
pub type Pair = Box<(Value, Value)>;

type ValueClone<T> = Rc<dyn Fn() -> T>;

pub enum Value {
    Number(Number, ValueClone<Number>),
    Bytes(Bytes, ValueClone<Bytes>),
    List(List, ValueClone<List>),
    Func(Func, ValueClone<Func>),
    Pair(Pair, ValueClone<Pair>),
    Name(String),
}

impl Clone for Value {
    fn clone(&self) -> Self {
        use Value::*;
        match self {
            Number(_, clone) => {
                let niw = clone();
                Number(niw, clone.clone())
            }
            Bytes(_, clone) => {
                let niw = clone();
                Bytes(niw, clone.clone())
            }
            List(_, clone) => {
                let niw = clone();
                List(niw, clone.clone())
            }
            Func(_, clone) => {
                let niw = clone();
                Func(niw, clone.clone())
            }
            Pair(_, clone) => {
                let niw = clone();
                Pair(niw, clone.clone())
            }
            Name(n) => Name(n.clone()),
        }
    }
}

macro_rules! make_value_unwrap {
    ($fname:ident -> $tname:ident) => {
        fn $fname(self) -> $tname {
            match self {
                Value::$tname(r, _) => r,
                Value::Name(name) => panic!("'{name}' was not added to interp"),
                _ => unreachable!(
                    "runtime type mismatchs should not be possible ({} where {} expected)",
                    self.kind_to_str(),
                    stringify!($tname)
                ),
            }
        }
    };
}

impl Value {
    fn kind_to_str(&self) -> &'static str {
        use Value::*;
        match self {
            Number(_, _) => "Number",
            Bytes(_, _) => "Bytes",
            List(_, _) => "List",
            Func(_, _) => "Func",
            Pair(_, _) => "Pair",
            Name(_) => "Name",
        }
    }

    make_value_unwrap!(number -> Number);
    make_value_unwrap!(bytes -> Bytes);
    make_value_unwrap!(list -> List);
    make_value_unwrap!(func -> Func);
    make_value_unwrap!(pair -> Pair);
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

// let pattern matching {{{
#[derive(Clone)]
enum Matcher {
    Bytes(Vec<u8>),
    Number(f64),
    List(Vec<Matcher>, Option<String>),
    Name(String),
    Pair(Box<Matcher>, Box<Matcher>),
}

impl Matcher {
    fn new(pat: &Pattern) -> Matcher {
        use Matcher::*;
        match pat {
            Pattern::Bytes(b) => Bytes(b.clone()),
            Pattern::Number(n) => Number(*n),
            Pattern::List(l, r) => List(
                l.iter().map(Matcher::new).collect(),
                r.as_ref().map(|r| r.1.clone()),
            ),
            Pattern::Name(_, n) => Name(n.clone()),
            Pattern::Pair(f, s) => Pair(Box::new(Matcher::new(f)), Box::new(Matcher::new(s))),
        }
    }

    fn matches_and_bind(self, v: Value, names: &mut HashMap<String, Value>) -> bool {
        match (self, v) {
            (Matcher::Bytes(b), Value::Bytes(it, _)) => it.take(b.len()).collect::<Vec<_>>() == b,
            (Matcher::Number(n), Value::Number(it, _)) => it() == n,
            (Matcher::List(l, r), Value::List(mut it, clone)) => {
                let len = l.len();
                for m in l {
                    let Some(true) = it.next().map(|it| m.matches_and_bind(it, names)) else {
                        return false;
                    };
                }
                match r {
                    Some(n) => {
                        let l = Value::List(it, Rc::new(move || Box::new(clone().skip(len))));
                        names.insert(n, l);
                        true
                    }
                    None => it.next().is_none(),
                }
            }
            (Matcher::Pair(f, s), Value::Pair(it, _)) => {
                f.matches_and_bind(it.0, names) && s.matches_and_bind(it.1, names)
            }
            (Matcher::Name(n), v) => {
                names.insert(n, v);
                true
            }
            _ => false,
        }
    }
}
// }}}

// lookup and interp {{{
fn lookup_builtin(name: &str) -> Value {
    match name {
        "pipe" => curried_value!(|f, g| -> Func |v| g.func()(f.func()(v))),

        "panic" => curried_value!(|| -> Func |t: Value| panic!("{}", String::from_utf8_lossy(&t.bytes().collect::<Vec<u8>>()))),

        "apply" => curried_value!(|f| -> Func |v| f.func()(v)),

        "add" => curried_value!(|a, b| -> Number || a.number()() + b.number()()),

        "const" => curried_value!(|a| -> Func |_| a),

        "div" => curried_value!(|a, b| -> Number || a.number()() / b.number()()),

        "duple" => curried_value!(|a| -> Pair (a.clone(), a.clone())),

        "enumerate" => curried_value!(|l| -> List l.list().enumerate().map(|(k, v)| {
            let fst = Value::Number(Box::new(move || k as f64), Rc::new(move || Box::new(move || k as f64)));
            let snd = v;
            let w = move || Box::new((fst.clone(), snd.clone()));
            Value::Pair(w(), Rc::new(w))
        })),

        "flip" => curried_value!(|f, b| -> Func |a| f.func()(a).func()(b)),

        "fold" => {
            curried_value!(|f_ba, b| -> Func move |l_a: Value| l_a.list().fold(b, |b, a| f_ba.clone().func()(b).func()(a)))
        }

        "head" => {
            curried_value!(|| -> Func |l: Value| l.list().next().unwrap_or_else(|| panic!("runtime panic (list access)")))
        }

        "init" => curried_value!(|l| -> List {
            let mut p = l.list().peekable();
            let mut c = p.next();
            iter::from_fn(move || {
                p.peek()?;
                mem::replace(&mut c, p.next())
            })
        }),

        "-" => curried_value!(|| -> Bytes {
            static SHARED: OnceLock<RwLock<(Stdin, Vec<u8>)>> = OnceLock::new();
            let mut at = 0;
            iter::from_fn(move || {
                let iv = SHARED
                    .get_or_init(|| RwLock::new((io::stdin(), Vec::new())))
                    .try_read()
                    .unwrap();
                if at < iv.1.len() {
                    at += 1;
                    Some(iv.1[at - 1])
                } else {
                    drop(iv);
                    let mut iv = SHARED.get().unwrap().try_write().unwrap();
                    let mut r = [0u8; 1];
                    iv.0.read_exact(&mut r).ok().map(|()| {
                        iv.1.push(r[0]);
                        at += 1;
                        r[0]
                    })
                }
            })
        }),

        "iterate" => curried_value!(|f, ini| -> List {
            let mut curr = ini;
            iter::from_fn(move || {
                let next = f.clone().func()(curr.clone());
                Some(mem::replace(&mut curr, next))
            })
        }),

        "join" => curried_value!(|sep, lst| -> Bytes {
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
        }),

        "last" => {
            curried_value!(|| -> Func |l: Value| l.list().last().unwrap_or_else(|| panic!("runtime panic (list access)")))
        }

        "len" => curried_value!(|list| -> Number || list.list().count() as f64),

        "ln" => curried_value!(|s| -> Bytes s.bytes().chain(iter::once(b'\n'))),

        "lookup" => curried_value!(|l| -> Func move |k: Value| {
            let k: Vec<_> = k.bytes().collect();
            match l.list().find_map(|p| {
                let p = p.pair();
                if p.0.bytes().collect::<Vec<_>>() == k {
                    Some(p.1)
                } else {
                    None
                }
            }) {
                Some(v) => Value::List(Box::new(iter::once(v.clone())), Rc::new(move || Box::new(iter::once(v.clone())))),
                None => Value::List(Box::new(iter::empty()), Rc::new(|| Box::new(iter::empty()))),
            }
        }),

        "map" => curried_value!(|f, l| -> List l.list().map(move |i| f.clone().func()(i))),

        "repeat" => curried_value!(|val| -> List iter::repeat(val)),

        "split" => curried_value!(|sep, s| -> List {
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
                let v = mem::take(&mut updog);
                let vv = v.clone();
                Some(Value::Bytes(
                    Box::new(v.into_iter()),
                    Rc::new(move || Box::new(vv.clone().into_iter())),
                ))
            })
        }),

        "tail" => curried_value!(|l| -> List l.list().skip(1)),

        "take" => curried_value!(|n, l| -> List l.list().take(n.number()() as usize)),

        "tonum" => curried_value!(|s| -> Number || {
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
        }),

        "tostr" => curried_value!(|n| -> Bytes n.number()().to_string().into_bytes().into_iter()),

        "zipwith" => {
            curried_value!(|f, la, lb| -> List la.list().zip(lb.list()).map(move |(a, b)| f.clone().func()(a).func()(b)))
        }

        // XXX: completely incorrect but just for messing around for now
        "uncodepoints" => curried_value!(|l| -> Bytes l.list().map(|n| n.number()() as u8)),

        // XXX: completely incorrect but just for messing around for now
        "codepoints" => curried_value!(|s| -> List s.bytes().map(|b| {
            let n = b as f64;
            Value::Number(Box::new(move || n), Rc::new(move || Box::new(move || n)))
        })),

        _ => Value::Name(name.into()),
    }
}

fn interp_impl(tree: &Tree, global: &Global, names: &HashMap<String, Value>) -> Value {
    use TreeKind::*;
    match &tree.value {
        Bytes(v) => {
            let vv = v.clone();
            Value::Bytes(
                Box::new(v.clone().into_iter()),
                Rc::new(move || Box::new(vv.clone().into_iter())),
            )
        }

        &Number(n) => Value::Number(Box::new(move || n), Rc::new(move || Box::new(move || n))),

        List(items) => {
            let v = items
                .iter()
                .map(|u| interp_impl(u, global, names))
                .collect::<Vec<_>>();
            let vv = v.clone();
            Value::List(
                Box::new(v.into_iter()),
                Rc::new(move || Box::new(vv.clone().into_iter())),
            )
        }

        Apply(app, args) => {
            let func =
                match app {
                    Applicable::Name(name) => names.get(name).cloned().unwrap_or_else(|| {
                        match global.scope.lookup(name).unwrap() {
                            ScopeItem::Builtin(_, _) => lookup_builtin(name),
                            ScopeItem::Defined(tree, _) => interp_impl(tree, global, names),
                            ScopeItem::Binding(_, _) => unreachable!(),
                        }
                    }),

                    Applicable::Bind(pat, res, fbk) => {
                        let pat = Matcher::new(pat);
                        let res = *res.clone();
                        let fbk = interp_impl(fbk, global, names);
                        let global_ptr = global as *const Global;
                        let names_at_point = names.clone();
                        let w = move || {
                            let pat = pat.clone();
                            let res = res.clone();
                            let fbk = fbk.clone();
                            let mut names = names_at_point.clone();
                            Box::new({
                                move |v| {
                                    if pat.matches_and_bind(v, &mut names) {
                                        interp_impl(&res, unsafe { &*global_ptr }, &names)
                                    } else {
                                        fbk
                                    }
                                }
                            })
                        };
                        Value::Func(w(), Rc::new(move || w()))
                    }
                };

            args.iter()
                .map(|u| interp_impl(u, global, names))
                .fold(func, |acc, cur| acc.func()(cur))
        }

        Pair(fst, snd) => {
            let fst = interp_impl(fst, global, names);
            let snd = interp_impl(snd, global, names);
            let w = move || Box::new((fst.clone(), snd.clone()));
            Value::Pair(w(), Rc::new(w))
        }
    }
}
// }}}

fn run_impl(val: Value, w: &mut impl Write) -> IoResult<()> {
    match val {
        Value::Number(n, _) => write!(w, "{}", n()),
        Value::Bytes(mut b, _) => b.try_for_each(|ch| w.write_all(&[ch])),
        Value::List(mut l, _) => l.try_for_each(|it| {
            run_impl(it, w)?;
            writeln!(w)
        }),
        Value::Func(_f, _) => panic!("run_print on a function value"),
        Value::Pair(p, _) => {
            run_impl(p.0, w)?;
            write!(w, "\t")?;
            run_impl(p.1, w)
        }
        Value::Name(name) => unreachable!("run_print on a name: '{name}'"),
    }
}
