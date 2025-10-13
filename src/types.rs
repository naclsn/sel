use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter::{self, Peekable};
use std::rc::{Rc, Weak};

use crate::error::{self, ErrorKind};

#[derive(Debug)]
pub enum Type {
    Known(Known),
    Named(String, RefCell<Vec<*mut Rc<Type>>>),
}

#[derive(Debug)]
enum Known {
    Number,
    Bytes(bool),          // XXX: boundedness!
    List(bool, Rc<Type>), // XXX: boundedness!
    Func(Rc<Type>, Rc<Type>),
    Pair(Rc<Type>, Rc<Type>),
}

use Known::*;

impl Display for Type {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            Type::Known(Number) => write!(f, "Num"),

            Type::Known(Bytes(true)) => write!(f, "Str"),
            Type::Known(Bytes(false)) => write!(f, "Str+"),

            Type::Known(List(true, has)) => write!(f, "[{has}]"),
            Type::Known(List(false, has)) => write!(f, "[{has}]+"),

            Type::Known(Func(par, ret)) => {
                let funcarg = matches!(**par, Type::Known(Func(_, _)));
                if funcarg {
                    write!(f, "(")?;
                }
                write!(f, "{par}")?;
                if funcarg {
                    write!(f, ")")?;
                }
                write!(f, " -> ")?;
                write!(f, "{ret}")
            }

            Type::Known(Pair(fst, snd)) => write!(f, "({fst}, {snd})"),

            Type::Named(name, _) => write!(f, "{name}"),
        }
    }
}

impl Type {
    pub fn number() -> Type {
        Type::Known(Number)
    }
    pub fn bytes(finite: bool) -> Type {
        Type::Known(Bytes(finite))
    }
    pub fn list(finite: bool, item: Rc<Type>) -> Type {
        Type::Known(List(finite, item))
    }
    pub fn func(par: Rc<Type>, ret: Rc<Type>) -> Type {
        Type::Known(Func(par, ret))
    }
    pub fn pair(fst: Rc<Type>, snd: Rc<Type>) -> Type {
        Type::Known(Pair(fst, snd))
    }
    pub fn named<'a>(name: String, refs: impl IntoIterator<Item = &'a Weak<Type>>) -> Type {
        Type::Named(
            name.to_string(),
            refs.into_iter()
                .map(|w| w.as_ptr() as *mut _)
                .collect::<Vec<_>>()
                .into(),
        )
    }

    // (ofc does not support pseudo-notations)
    // TODO: could be made into supporting +1, +2.. notation
    // XXX: this is all p bad
    // FIXME: does it even actively handles pairs?
    pub fn parse_str(&mut self, s: &str) -> Option<Rc<Type>> {
        enum Tok {
            Arrow, // "->"
            Num,
            Str,
            Punct(u8), // ()+,[]
            Name(Rc<Type>),
        }

        let mut nameds = HashMap::new();
        let mut s = s.bytes().peekable();
        let tokens: Vec<_> = iter::from_fn(|| match s.find(|c: &u8| !c.is_ascii_whitespace())? {
            b'-' => s.next().filter(|c| b'>' == *c).map(|_| Tok::Arrow),

            b'N' => s
                .next()
                .zip(s.next())
                .filter(|p| (b'u', b'm') == *p)
                .map(|_| Tok::Num),
            b'S' => s
                .next()
                .zip(s.next())
                .filter(|p| (b't', b'r') == *p)
                .map(|_| Tok::Str),

            c @ (b'(' | b')' | b'+' | b',' | b'[' | b']') => Some(Tok::Punct(c)),

            c @ b'a'..=b'z' => {
                let mut name = vec![c];
                name.extend(iter::from_fn(|| s.next_if(u8::is_ascii_lowercase)));
                let name = unsafe { String::from_utf8_unchecked(name.clone()) };
                nameds
                    .get(&name)
                    .map(|ty: &Rc<Type>| Tok::Name(ty.clone()))
                    .or_else(|| {
                        let named: Rc<_> = Type::named(name.clone(), []).into();
                        nameds.insert(name, named.clone());
                        Some(Tok::Name(named))
                    })
            }

            _ => None,
        })
        .collect();

        fn _impl(tokens: &mut Peekable<impl Iterator<Item = Tok>>) -> Option<Rc<Type>> {
            let ty: Rc<_> = match tokens.next()? {
                Tok::Num => Type::number().into(),
                Tok::Str => {
                    let fin = tokens.next_if(|t| matches!(t, Tok::Punct(b'+'))).is_none();
                    Type::bytes(fin).into()
                }

                Tok::Punct(b'[') => {
                    let item = _impl(tokens)?;
                    tokens.next_if(|t| matches!(t, Tok::Punct(b']')))?;
                    let fin = tokens.next_if(|t| matches!(t, Tok::Punct(b'+'))).is_none();
                    Type::list(fin, item).into()
                }

                Tok::Punct(b'(') => {
                    let fst = _impl(tokens)?;
                    let r = if tokens.next_if(|t| matches!(t, Tok::Punct(b','))).is_some() {
                        let snd = _impl(tokens)?;
                        Type::pair(fst, snd).into()
                    } else {
                        fst
                    };
                    tokens.next_if(|t| matches!(t, Tok::Punct(b')')))?;
                    r
                }

                Tok::Name(ty) => ty,

                _ => return None,
            };

            Some(if tokens.next_if(|t| matches!(t, Tok::Arrow)).is_some() {
                Type::func(ty, _impl(tokens)?).into()
            } else {
                ty
            })
        }

        _impl(&mut tokens.into_iter().peekable())
    }

    pub fn deep_clone(this: &Rc<Type>) -> Rc<Type> {
        fn _inner(
            ty: &Rc<Type>,
            as_part_of: *mut Rc<Type>,
            already_done: &mut HashMap<*const Type, Rc<Type>>,
        ) -> Rc<Type> {
            if let Some(done) = already_done.get(&Rc::as_ptr(ty)) {
                if let Type::Named(_, refs) = done.as_ref() {
                    assert!(!as_part_of.is_null());
                    refs.borrow_mut().push(as_part_of);
                }
                return done.clone();
            }

            let r = match ty.as_ref() {
                Type::Known(known) => Rc::new_cyclic(|w| {
                    let mut f = |t| _inner(t, w.as_ptr() as *mut _, already_done);
                    Type::Known(match known {
                        Number => Number,
                        Bytes(fin) => Bytes(*fin),
                        List(fin, it) => List(*fin, f(it)),
                        Func(par, ret) => Func(f(par), f(ret)),
                        Pair(fst, snd) => Pair(f(fst), f(snd)),
                    })
                }),

                Type::Named(n, _) => Type::Named(n.clone(), vec![].into()).into(),
            };

            already_done.insert(Rc::as_ptr(ty), r.clone());
            return r;
        }

        // handle this case here so that within `_inner` we always have a `as_part_of` for this
        if let Type::Named(n, _) = this.as_ref() {
            Type::Named(n.clone(), vec![].into()).into()
        } else {
            _inner(
                this,
                std::ptr::null::<Rc<Type>>() as *mut _,
                &mut Default::default(),
            )
        }
    }

    /// `func give` so return type of `func` (if checks out).
    pub(crate) fn applied(func: &Rc<Type>, give: &Rc<Type>) -> Result<Rc<Type>, ErrorKind> {
        match func.as_ref() {
            Type::Known(Func(want, ret)) => Type::concretize(want, give).map(|()| ret.clone()),
            _ => Err(error::not_function(func)),
        }
    }

    /// Concretizes boundedness and unknown named types:
    /// * fin <- fin is fin
    /// * inf <- inf is inf
    /// * inf <- fin is fin
    /// * unk <- Knw is Knw
    ///
    /// Both types may be modified (due to contravariance of func in parameter type).
    /// This uses the fact that inf types and unk named types are
    /// only depended on by the functions that introduced them.
    fn concretize(want: &Rc<Type>, give: &Rc<Type>) -> Result<(), ErrorKind> {
        match (want.as_ref(), give.as_ref()) {
            (Type::Known(Number), Type::Known(Number)) => Ok(()),

            // XXX: boundedness
            (Type::Known(Bytes(_)), Type::Known(Bytes(_))) => Ok(()),

            // XXX: boundedness
            (Type::Known(List(_, l_ty)), Type::Known(List(_, r_ty))) => {
                Type::concretize(l_ty, r_ty)
            }

            (Type::Known(Func(l_par, l_ret)), Type::Known(Func(r_par, r_ret))) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str+ -> Str+)
                // (a -> b) <- (c -> c)
                Type::concretize(l_ret, r_ret)?;
                Type::concretize(r_par, l_par)?;
                // needs to propagate back again any change by 2nd concretize
                // XXX: rly? i think but idk
                //Type::concretize(l_ret, r_ret)
                Ok(())
            }

            (Type::Known(Pair(l_fst, l_snd)), Type::Known(Pair(r_fst, r_snd))) => {
                Type::concretize(l_fst, r_fst)?;
                Type::concretize(l_snd, r_snd)
            }

            (Type::Named(w, w_refs), Type::Named(g, g_refs)) => {
                // when we have ptr equality, we know we are talking twice about
                // the exact same named, in which case nothing needs to be done
                if !std::ptr::eq(w, g) {
                    let mut all = w_refs.take();
                    all.extend(g_refs.take());

                    let niw = Rc::new(Type::Named(
                        if w == g {
                            w.clone()
                        } else {
                            format!("{w}={g}")
                        },
                        all.clone().into(),
                    ));

                    for r in all {
                        *(unsafe { &mut *r }) = niw.clone();
                    }
                }
                Ok(())
            }
            (_known, Type::Named(_, refs)) => {
                for r in refs.take() {
                    *(unsafe { &mut *r }) = want.clone();
                }
                Ok(())
            }
            (Type::Named(_, refs), _known) => {
                // all that was referring to 'want' now should refer to 'give'
                for r in refs.take() {
                    //unsafe {
                    //    Rc::decrement_strong_count(Rc::into_raw(*r));
                    //    *r = known.clone();
                    //}
                    // i *think* this should do the same
                    // cvt to a normal managed ref will have it behave regularly and decrement rc
                    *(unsafe { &mut *r }) = give.clone();
                }
                // rc should have hit 0 for 'want' (not sure how to check)
                Ok(())
            }

            _ => Err(error::type_mismatch(want, give)),
        }
    }
}
