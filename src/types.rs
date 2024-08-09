use std::fmt::{Display, Formatter, Result as FmtResult};

use crate::error::Error;

pub(crate) const ERROR_TYPE: &str = "{error}";
pub(crate) const NUMBER_TYPEREF: usize = 0;
pub(crate) const STRFIN_TYPEREF: usize = 1;
pub(crate) const FINITE_TYPEREF: usize = 2;

pub type TypeRef = usize;
pub type Boundedness = usize;

#[derive(PartialEq, Debug, Clone)]
pub(crate) enum Type {
    Number,
    Bytes(Boundedness),
    List(Boundedness, TypeRef),
    Func(TypeRef, TypeRef),
    Named(String),
    Finite(bool),
}

#[derive(PartialEq, Debug, Clone)]
pub enum FrozenType {
    Number,
    Bytes(bool),
    List(bool, Box<FrozenType>),
    Func(Box<FrozenType>, Box<FrozenType>),
    Named(String),
}

#[derive(PartialEq, Debug, Clone)]
pub struct TypeList(Vec<Option<Type>>);
pub struct TypeRepr<'a>(TypeRef, &'a TypeList);

impl Default for TypeList {
    fn default() -> Self {
        TypeList(vec![
            Some(Type::Number),       // [NUMBER_TYPEREF]
            Some(Type::Bytes(2)),     // [STRFIN_TYPEREF]
            Some(Type::Finite(true)), // [FINITE_TYPEREF]
        ])
    }
}

// types vec with holes, also factory and such {{{
impl TypeList {
    fn push(&mut self, it: Type) -> TypeRef {
        if let Some((k, o)) = self.0.iter_mut().enumerate().find(|(_, o)| o.is_none()) {
            *o = Some(it);
            k
        } else {
            self.0.push(Some(it));
            self.0.len() - 1
        }
    }

    pub fn number(&self) -> TypeRef {
        NUMBER_TYPEREF
    }
    pub fn bytes(&mut self, finite: Boundedness) -> TypeRef {
        if FINITE_TYPEREF == finite {
            STRFIN_TYPEREF
        } else {
            self.push(Type::Bytes(finite))
        }
    }
    pub fn list(&mut self, finite: Boundedness, item: TypeRef) -> TypeRef {
        self.push(Type::List(finite, item))
    }
    pub fn func(&mut self, par: TypeRef, ret: TypeRef) -> TypeRef {
        self.push(Type::Func(par, ret))
    }
    pub fn named(&mut self, name: &str) -> TypeRef {
        self.push(Type::Named(name.to_string()))
    }
    pub fn finite(&self) -> Boundedness {
        FINITE_TYPEREF
    }
    pub fn infinite(&mut self) -> Boundedness {
        self.push(Type::Finite(false))
    }

    fn get(&self, at: TypeRef) -> &Type {
        self.0[at].as_ref().unwrap()
    }

    fn get_mut(&mut self, at: TypeRef) -> &mut Type {
        self.0[at].as_mut().unwrap()
    }

    pub fn repr(&self, at: TypeRef) -> TypeRepr {
        TypeRepr(at, self)
    }

    #[allow(dead_code)]
    pub fn frozen(&self, at: TypeRef) -> FrozenType {
        match self.get(at) {
            Type::Number => FrozenType::Number,
            Type::Bytes(b) => FrozenType::Bytes(match self.get(*b) {
                Type::Finite(b) => *b,
                _ => unreachable!(),
            }),
            Type::List(b, items) => FrozenType::List(
                match self.get(*b) {
                    Type::Finite(b) => *b,
                    _ => unreachable!(),
                },
                Box::new(self.frozen(*items)),
            ),
            Type::Func(par, ret) => {
                FrozenType::Func(Box::new(self.frozen(*par)), Box::new(self.frozen(*ret)))
            }
            Type::Named(name) => FrozenType::Named(name.clone()),
            Type::Finite(_) => unreachable!(),
        }
    }

    //pub fn pop(&mut self, at: TypeRef) -> Type {
    //    let r = self.0[at].take().unwrap();
    //    while let Some(None) = self.0.last() {
    //        self.0.pop();
    //    }
    //    r
    //}
}
// }}}

// type manipulation {{{
impl Type {
    /// `func give` so returns type of `func` (if checks out).
    pub(crate) fn applied(
        func: TypeRef,
        give: TypeRef,
        types: &mut TypeList,
    ) -> Result<TypeRef, Error> {
        match types.get(func) {
            &Type::Func(want, ret) => Type::concretize(want, give, types).map(|()| ret),
            Type::Named(err) if ERROR_TYPE == err => Ok(func),
            _ => Err(Error::NotFunc(func)),
        }
    }

    /// Same idea but doesn't mutate anything and doesn't fail but returns true/false.
    pub(crate) fn applicable(func: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        match types.get(func) {
            &Type::Func(want, _) => Type::compatible(want, give, types),
            Type::Named(err) if ERROR_TYPE == err => true,
            _ => false,
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
    fn concretize(want: TypeRef, give: TypeRef, types: &mut TypeList) -> Result<(), Error> {
        use Type::*;
        match (types.get(want), types.get(give)) {
            (Named(err), _) if ERROR_TYPE == err => {
                // XXX: I think here it doesn't make sense
                //*types.get_mut(want) = other.clone();
                Ok(())
            }

            (Number, Number) => Ok(()),

            (Bytes(fw), Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => Ok(()),
                (Finite(false), Finite(true)) => {
                    *types.get_mut(*fw) = Finite(true);
                    Ok(())
                }
                (Finite(true), Finite(false)) => {
                    // XXX: I think here it doesn't make sense
                    //*types.get_mut(want) = Type::Named(ERROR_TYPE.into());
                    Err(Error::InfWhereFinExpected)
                }
                _ => unreachable!(),
            },

            (List(fw, l_ty), List(fg, r_ty)) => {
                let (l_ty, r_ty) = (*l_ty, *r_ty);
                match (types.get(*fw), types.get(*fg)) {
                    (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => (),
                    (Finite(false), Finite(true)) => *types.get_mut(*fw) = Finite(true),
                    (Finite(true), Finite(false)) => {
                        // XXX: I think here it doesn't make sense
                        //*types.get_mut(want) = Type::Named(ERROR_TYPE.into());
                        return Err(Error::InfWhereFinExpected);
                    }
                    _ => unreachable!(),
                }
                Type::concretize(l_ty, r_ty, types)
            }

            (&Func(l_par, l_ret), &Func(r_par, r_ret)) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str* -> Str*)
                // (a -> b) <- (c -> c)
                Type::concretize(r_par, l_par, types)?;
                Type::concretize(l_ret, r_ret, types)
            }

            (Named(_), other) => {
                *types.get_mut(want) = other.clone();
                Ok(())
            }
            (other, Named(_)) => {
                *types.get_mut(give) = other.clone();
                Ok(())
            }

            _ => {
                let pwant = types.push(types.get(want).clone());
                *types.get_mut(want) = Type::Named(ERROR_TYPE.into());
                Err(Error::ExpectedButGot(pwant, give))
            }
        }
    }

    /// Not sure what is this operation;
    /// it's not union because it is asymmetric..
    /// it's used when finding the item type of a list.
    ///
    /// ```plaintext
    /// a | [Num]* | [Num] ->> [Num]*
    /// a | Str | Num ->> never
    /// a | Str* | Str ->> Str*
    /// a | Str | Str* ->> never // XXX: disputable, same with []|[]*
    /// a | [Str*] | [Str] ->> [Str*]
    /// a | (Str -> c) | (Str* -> Str*) ->> (Str -> Str*)
    /// a | ([Str*] -> x) | ([Str] -> x) ->> ([Str] -> x)
    /// ```
    pub(crate) fn harmonize(
        curr: TypeRef,
        item: TypeRef,
        types: &mut TypeList,
    ) -> Result<(), Error> {
        use Type::*;
        match (types.get(curr), types.get(item)) {
            (Named(err), _) if ERROR_TYPE == err => Ok(()),

            (Number, Number) => Ok(()),

            (Bytes(fw), Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => Ok(()),
                (Finite(false), Finite(true)) => Ok(()),
                (Finite(true), Finite(false)) => {
                    *types.get_mut(curr) = Type::Named(ERROR_TYPE.into());
                    Err(Error::InfWhereFinExpected)
                }
                _ => unreachable!(),
            },

            (List(fw, l_ty), List(fg, r_ty)) => {
                let (l_ty, r_ty) = (*l_ty, *r_ty);
                match (types.get(*fw), types.get(*fg)) {
                    (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => (),
                    (Finite(false), Finite(true)) => (),
                    (Finite(true), Finite(false)) => {
                        *types.get_mut(curr) = Type::Named(ERROR_TYPE.into());
                        return Err(Error::InfWhereFinExpected);
                    }
                    _ => unreachable!(),
                }
                Type::harmonize(l_ty, r_ty, types)
            }

            (&Func(l_par, l_ret), &Func(r_par, r_ret)) => {
                // contravariance
                Type::harmonize(r_par, l_par, types)?;
                Type::harmonize(l_ret, r_ret, types)
            }

            (Named(_), other) => {
                *types.get_mut(curr) = other.clone();
                Ok(())
            }
            (other, Named(_)) => {
                *types.get_mut(item) = other.clone();
                Ok(())
            }

            _ => {
                let pcurr = types.push(types.get(curr).clone());
                *types.get_mut(curr) = Type::Named(ERROR_TYPE.into());
                Err(Error::ExpectedButGot(pcurr, item))
            }
        }
    }

    fn compatible(want: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        use Type::*;
        match (types.get(want), types.get(give)) {
            (Number, Number) => true,

            (Bytes(fw), Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Finite(true), Finite(false)) => false,
                (Finite(_), Finite(_)) => true,
                _ => unreachable!(),
            },

            (List(fw, l_ty), List(fg, r_ty)) => match (types.get(*fw), types.get(*fg)) {
                (Finite(true), Finite(false)) => false,
                (Finite(_), Finite(_)) => Type::compatible(*l_ty, *r_ty, types),
                _ => unreachable!(),
            },

            (Func(l_par, l_ret), Func(r_par, r_ret)) => {
                // parameter compare contravariantly: (Str -> c) <- (Str* -> Str*)
                Type::compatible(*r_par, *l_par, types) && Type::compatible(*l_ret, *r_ret, types)
            }

            (Named(_), _) => true,
            (_, Named(_)) => true,

            _ => false,
        }
    }
}
// }}}

impl Display for TypeRepr<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        let types = &self.1;
        match types.get(self.0) {
            Type::Number => write!(f, "Num"),

            Type::Bytes(b) => write!(f, "Str{}", types.repr(*b)),

            Type::List(b, has) => write!(f, "[{}]{}", types.repr(*has), types.repr(*b)),

            Type::Func(par, ret) => {
                let funcarg = matches!(types.get(*par), Type::Func(_, _));
                if funcarg {
                    write!(f, "(")?;
                }
                write!(f, "{}", types.repr(*par))?;
                if funcarg {
                    write!(f, ")")?;
                }
                write!(f, " -> ")?;
                write!(f, "{}", types.repr(*ret))
            }

            Type::Named(name) => write!(f, "{name}"),

            Type::Finite(true) => Ok(()),
            Type::Finite(false) => write!(f, "*"),
        }
    }
}
