use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter::{self, Peekable};
use std::rc::Rc;

use crate::errors::{self, Error};
use crate::module::Location;

#[derive(Debug)]
pub enum Type {
    Number,
    Bytes(bool),          // XXX: boundedness!
    List(bool, Rc<Type>), // XXX: boundedness!
    Func(Rc<Type>, Rc<Type>),
    Pair(Rc<Type>, Rc<Type>),
    Named(String, RefCell<Option<Rc<Type>>>),
}

use Type::*;

//#[derive(Debug)]
//pub struct Rc<Type>(Rc<Type>);
//
//impl Deref for Rc<Type> {
//    type Target = Type;
//
//    fn deref(&self) -> &Self::Target {
//        match &*self.0 {
//            Named(_, Some(ty)) => ty.borrow().deref(),
//            ty => ty,
//        }
//    }
//}

impl Display for Type {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            Number => write!(f, "Num"),

            Bytes(true) => write!(f, "Str"),
            Bytes(false) => write!(f, "Str+"),

            List(true, has) => write!(f, "[{has}]"),
            List(false, has) => write!(f, "[{has}]+"),

            Func(par, ret) => {
                let funcarg = matches!(**par, Func(_, _));
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

            Pair(fst, snd) => write!(f, "({fst}, {snd})"),

            Named(name, tr) => {
                if let Some(ty) = &*tr.borrow() {
                    write!(f, "{}", ty)
                } else {
                    write!(f, "{name}")
                }
            }
        }
    }
}

impl Type {
    pub fn number() -> Rc<Type> {
        Number.into()
    }
    pub fn bytes(finite: bool) -> Rc<Type> {
        Bytes(finite).into()
    }
    pub fn list(finite: bool, item: Rc<Type>) -> Rc<Type> {
        List(finite, item).into()
    }
    pub fn func(par: Rc<Type>, ret: Rc<Type>) -> Rc<Type> {
        Func(par, ret).into()
    }
    pub fn pair(fst: Rc<Type>, snd: Rc<Type>) -> Rc<Type> {
        Pair(fst, snd).into()
    }
    pub fn named(name: String) -> Rc<Type> {
        Named(name, None.into()).into()
    }
    pub fn named_to(name: String, tr: Rc<Type>) -> Rc<Type> {
        Named(name, Some(tr).into()).into()
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
                        let named: Rc<Type> = Type::named(name.clone()).into();
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

    pub fn deep_clone(&self) -> Rc<Type> {
        fn _inner(ty: &Type, already: &mut HashMap<*const Type, Rc<Type>>) -> Rc<Type> {
            if let Some(r) = already.get(&(ty as *const _)) {
                return r.clone();
            }

            let r = match ty {
                Number => Type::number(),
                Bytes(fin) => Type::bytes(*fin),
                List(fin, it) => Type::list(*fin, _inner(it, already)),
                Func(par, ret) => Type::func(_inner(par, already), _inner(ret, already)),
                Pair(fst, snd) => Type::pair(_inner(fst, already), _inner(snd, already)),
                Named(n, tr) => {
                    if let Some(ty) = &*tr.borrow() {
                        // XXX: for now keeping the tr (caries the names),
                        //      otherwise should just be the `_inner..` bit
                        Type::named_to(n.clone(), _inner(ty, already))
                    } else {
                        Type::named(n.clone())
                    }
                }
            };

            already.insert(ty as *const _, r.clone());
            return r;
        }

        _inner(self, &mut Default::default())
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
    pub fn concretize(
        loc_func: Location,
        want: &Rc<Type>,
        loc_arg: Location,
        give: &Rc<Type>,
    ) -> Result<(), Error> {
        match (&**want, &**give) {
            (Number, Number) => Ok(()),

            // XXX: boundedness
            (Bytes(_), Bytes(_)) => Ok(()),

            // XXX: boundedness
            (List(_, l_ty), List(_, r_ty)) => Type::concretize(loc_func, l_ty, loc_arg, r_ty),

            (Func(l_par, l_ret), Func(r_par, r_ret)) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str+ -> Str+)
                // (a -> b) <- (c -> c)
                Type::concretize(loc_func.clone(), l_ret, loc_arg.clone(), r_ret)?;
                Type::concretize(loc_func, r_par, loc_arg, l_par)?;
                // needs to propagate back again any change by 2nd concretize
                // XXX: rly? i think but idk
                //Type::concretize(l_ret, r_ret)
                Ok(())
            }

            (Pair(l_fst, l_snd), Pair(r_fst, r_snd)) => {
                Type::concretize(loc_func.clone(), l_fst, loc_arg.clone(), r_fst)?;
                Type::concretize(loc_func, l_snd, loc_arg, r_snd)
            }

            (Named(w, w_tr), Named(g, g_tr)) => {
                // when we have ptr equality, we know we are talking twice about
                // the exact same named, in which case nothing needs to be done
                if std::ptr::eq(w, g) {
                    Ok(())
                } else {
                    let w_borrow = w_tr.borrow();
                    let g_borrow = g_tr.borrow();
                    match (&*w_borrow, &*g_borrow) {
                        (None, None) => {
                            drop(w_borrow);
                            drop(g_borrow);
                            // XXX: should just point on to the other,
                            //      for now keeping this for the names (a, b and a=b)
                            let niw = Type::named(if w == g {
                                w.clone()
                            } else {
                                format!("{w}={g}")
                            });
                            *w_tr.borrow_mut() = Some(niw.clone());
                            *g_tr.borrow_mut() = Some(niw);
                            Ok(())
                        }
                        (None, Some(give)) => {
                            drop(w_borrow);
                            Type::concretize(loc_func, want, loc_arg, give)
                        }
                        (Some(want), None) => {
                            drop(g_borrow);
                            Type::concretize(loc_func, want, loc_arg, give)
                        }
                        (Some(want), Some(give)) => Type::concretize(loc_func, want, loc_arg, give),
                    }
                }
            }

            (_known, Named(_, g_tr)) => {
                let g_borrow = g_tr.borrow();
                if let Some(give) = &*g_borrow {
                    Type::concretize(loc_func, want, loc_arg, give)
                } else {
                    drop(g_borrow);
                    *g_tr.borrow_mut() = Some(want.clone());
                    Ok(())
                }
            }
            (Named(_, w_tr), _known) => {
                let w_borrow = w_tr.borrow();
                if let Some(want) = &*w_borrow {
                    Type::concretize(loc_func, want, loc_arg, give)
                } else {
                    drop(w_borrow);
                    *w_tr.borrow_mut() = Some(give.clone());
                    Ok(())
                }
            }

            _ => Err(errors::type_mismatch(loc_func, want, loc_arg, give)),
        }
    }
}

#[test]
fn test() {
    //todo!("test deep_clone, concretize, .. idk");
}
