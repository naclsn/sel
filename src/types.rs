use std::cell::RefCell;
use std::collections::HashMap;
use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter::{self, Peekable};
use std::rc::Rc;

use crate::errors::MismatchAs;

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

type TypeMismatch = (Rc<Type>, Rc<Type>, Vec<MismatchAs>);

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
        let tokens = iter::from_fn(|| match s.find(|c: &u8| !c.is_ascii_whitespace())? {
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
                        let named = Type::named(name.clone());
                        nameds.insert(name, named.clone());
                        Some(Tok::Name(named))
                    })
            }

            _ => None,
        });

        fn _impl(tokens: &mut Peekable<impl Iterator<Item = Tok>>) -> Option<Rc<Type>> {
            let ty: Rc<_> = match tokens.next()? {
                Tok::Num => Type::number(),
                Tok::Str => {
                    let fin = tokens.next_if(|t| matches!(t, Tok::Punct(b'+'))).is_none();
                    Type::bytes(fin)
                }

                Tok::Punct(b'[') => {
                    let item = _impl(tokens)?;
                    tokens.next_if(|t| matches!(t, Tok::Punct(b']')))?;
                    let fin = tokens.next_if(|t| matches!(t, Tok::Punct(b'+'))).is_none();
                    Type::list(fin, item)
                }

                Tok::Punct(b'(') => {
                    let fst = _impl(tokens)?;
                    let r = if tokens.next_if(|t| matches!(t, Tok::Punct(b','))).is_some() {
                        let snd = _impl(tokens)?;
                        Type::pair(fst, snd)
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
                Type::func(ty, _impl(tokens)?)
            } else {
                ty
            })
        }

        _impl(&mut tokens.peekable())
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
            r
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
    pub fn concretize(want: &Rc<Type>, give: &Rc<Type>) -> Result<(), TypeMismatch> {
        const fn frag(frag: MismatchAs) -> impl FnOnce(TypeMismatch) -> TypeMismatch {
            |(w, g, mut as_of)| {
                as_of.push(frag);
                (w, g, as_of)
            }
        }
        use MismatchAs::*;

        match (&**want, &**give) {
            (Number, Number) => Ok(()),

            // XXX: boundedness
            (Bytes(_), Bytes(_)) => Ok(()),

            // XXX: boundedness
            (List(_, w_ty), List(_, g_ty)) => Type::concretize(w_ty, g_ty).map_err(frag(ItemOf)),

            (Func(w_par, w_ret), Func(g_par, g_ret)) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str+ -> Str+)
                // (a -> b) <- (c -> c)
                Type::concretize(w_ret, g_ret).map_err(frag(RetOf))?;
                Type::concretize(g_par, w_par).map_err(frag(ParOf))?;
                // needs to propagate back again any change by 2nd concretize
                // XXX: rly? i think but idk
                //Type::concretize(w_ret, g_ret)
                Ok(())
            }

            (Pair(w_fst, w_snd), Pair(g_fst, g_snd)) => {
                Type::concretize(w_fst, g_fst).map_err(frag(FstOf))?;
                Type::concretize(w_snd, g_snd).map_err(frag(SndOf))
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
                            Type::concretize(want, give)
                        }
                        (Some(want), None) => {
                            drop(g_borrow);
                            Type::concretize(want, give)
                        }
                        (Some(want), Some(give)) => Type::concretize(want, give),
                    }
                }
            }

            (_known, Named(_, g_tr)) => {
                let g_borrow = g_tr.borrow();
                if let Some(give) = &*g_borrow {
                    Type::concretize(want, give)
                } else {
                    drop(g_borrow);
                    *g_tr.borrow_mut() = Some(want.clone());
                    Ok(())
                }
            }
            (Named(_, w_tr), _known) => {
                let w_borrow = w_tr.borrow();
                if let Some(want) = &*w_borrow {
                    Type::concretize(want, give)
                } else {
                    drop(w_borrow);
                    *w_tr.borrow_mut() = Some(give.clone());
                    Ok(())
                }
            }

            _ => Err((want.clone(), give.clone(), vec![])),
        }
    }
}

#[test]
fn test() {
    //todo!("test deep_clone, concretize, .. idk");
}
