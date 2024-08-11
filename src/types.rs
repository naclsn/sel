use std::fmt::{Display, Formatter, Result as FmtResult};

use crate::error::ErrorKind;

pub(crate) const ERROR_TYPE_NAME: &str = "{error}";
pub(crate) const NUMBER_TYPEREF: usize = 0;
pub(crate) const STRFIN_TYPEREF: usize = 1;
pub(crate) const FINITE_TYPEREF: usize = 2;
pub(crate) const ERROR_TYPEREF: usize = 3;

pub type TypeRef = usize;
pub type Boundedness = usize;

#[derive(Clone)]
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

#[derive(Clone)]
pub struct TypeList(Vec<Option<Type>>);

impl Default for TypeList {
    fn default() -> Self {
        TypeList(vec![
            Some(Type::Number),                        // [NUMBER_TYPEREF]
            Some(Type::Bytes(2)),                      // [STRFIN_TYPEREF]
            Some(Type::Finite(true)),                  // [FINITE_TYPEREF]
            Some(Type::Named(ERROR_TYPE_NAME.into())), // [ERROR_TYPEREF]
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

    pub fn get(&self, at: TypeRef) -> &Type {
        self.0[at].as_ref().unwrap()
    }

    fn get_mut(&mut self, at: TypeRef) -> &mut Type {
        self.0[at].as_mut().unwrap()
    }

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
    ) -> Result<TypeRef, ErrorKind> {
        match types.get(func) {
            &Type::Func(want, ret) => Type::concretize(want, give, types).map(|()| ret),
            Type::Named(err) if ERROR_TYPE_NAME == err => Ok(func),
            _ => Err(ErrorKind::NotFunc(types.frozen(func))),
        }
    }

    /// Same idea as `applied` but doesn't mutate anything and doesn't fail but returns true/false.
    pub(crate) fn applicable(func: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        match types.get(func) {
            &Type::Func(want, _) => Type::compatible(want, give, types),
            Type::Named(err) if ERROR_TYPE_NAME == err => true,
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
    fn concretize(want: TypeRef, give: TypeRef, types: &mut TypeList) -> Result<(), ErrorKind> {
        use Type::*;
        match (types.get(want), types.get(give)) {
            (Named(err), _) if ERROR_TYPE_NAME == err => Ok(()),

            (Number, Number) => Ok(()),

            (Bytes(fw), Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => Ok(()),
                (Finite(false), Finite(true)) => {
                    *types.get_mut(*fw) = Finite(true);
                    Ok(())
                }
                (Finite(true), Finite(false)) => Err(ErrorKind::InfWhereFinExpected),
                _ => unreachable!(),
            },

            (List(fw, l_ty), List(fg, r_ty)) => {
                let (l_ty, r_ty) = (*l_ty, *r_ty);
                match (types.get(*fw), types.get(*fg)) {
                    (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => (),
                    (Finite(false), Finite(true)) => *types.get_mut(*fw) = Finite(true),
                    (Finite(true), Finite(false)) => return Err(ErrorKind::InfWhereFinExpected),
                    _ => unreachable!(),
                }
                Type::concretize(l_ty, r_ty, types)
            }

            (&Func(l_par, l_ret), &Func(r_par, r_ret)) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str+ -> Str+)
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
                *types.get_mut(want) = Type::Named(ERROR_TYPE_NAME.into());
                Err(ErrorKind::ExpectedButGot(
                    types.frozen(pwant),
                    types.frozen(give),
                ))
            }
        }
    }

    /// Not sure what is this operation;
    /// it's not union because it is asymmetric..
    /// it's used when finding the item type of a list.
    ///
    /// ```plaintext
    /// a | [Num]+ | [Num] ->> [Num]+
    /// a | Str | Num ->> never
    /// a | Str+ | Str ->> Str+
    /// a | Str | Str+ ->> never // xxx: disputable, same with []|[]+
    /// a | [Str+] | [Str] ->> [Str+]
    /// a | (Str -> c) | (Str+ -> Str+) ->> (Str -> Str+)
    /// a | ([Str+] -> x) | ([Str] -> x) ->> ([Str] -> x)
    /// ```
    pub(crate) fn harmonize(
        curr: TypeRef,
        item: TypeRef,
        types: &mut TypeList,
    ) -> Result<(), ErrorKind> {
        use Type::*;
        match (types.get(curr), types.get(item)) {
            (Named(err), _) if ERROR_TYPE_NAME == err => Ok(()),

            (Number, Number) => Ok(()),

            (Bytes(fw), Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => Ok(()),
                (Finite(false), Finite(true)) => Ok(()),
                (Finite(true), Finite(false)) => {
                    *types.get_mut(curr) = Type::Named(ERROR_TYPE_NAME.into());
                    Err(ErrorKind::InfWhereFinExpected)
                }
                _ => unreachable!(),
            },

            (List(fw, l_ty), List(fg, r_ty)) => {
                let (l_ty, r_ty) = (*l_ty, *r_ty);
                match (types.get(*fw), types.get(*fg)) {
                    (Finite(fw_bool), Finite(fg_bool)) if fw_bool == fg_bool => (),
                    (Finite(false), Finite(true)) => (),
                    (Finite(true), Finite(false)) => {
                        *types.get_mut(curr) = Type::Named(ERROR_TYPE_NAME.into());
                        return Err(ErrorKind::InfWhereFinExpected);
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
                *types.get_mut(curr) = Type::Named(ERROR_TYPE_NAME.into());
                Err(ErrorKind::ExpectedButGot(
                    types.frozen(pcurr),
                    types.frozen(item),
                ))
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
                // parameter compare contravariantly: (Str -> c) <- (Str+ -> Str+)
                Type::compatible(*r_par, *l_par, types) && Type::compatible(*l_ret, *r_ret, types)
            }

            (Named(_), _) => true,
            (_, Named(_)) => true,

            _ => false,
        }
    }
}
// }}}

impl Display for FrozenType {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            FrozenType::Number => write!(f, "Num"),

            FrozenType::Bytes(true) => write!(f, "Str"),
            FrozenType::Bytes(false) => write!(f, "Str+"),

            FrozenType::List(true, has) => write!(f, "[{has}]"),
            FrozenType::List(false, has) => write!(f, "[{has}]+"),

            FrozenType::Func(par, ret) => {
                let funcarg = matches!(**par, FrozenType::Func(_, _));
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

            FrozenType::Named(name) => write!(f, "{name}"),
        }
    }
}
