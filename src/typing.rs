use std::io::{Error as IoError, Write};

use crate::parse::Error;

type Boundedness = usize;

#[derive(PartialEq, Debug, Clone)]
pub enum Type {
    Number,
    Bytes(Boundedness),
    List(Boundedness, usize),
    Func(usize, usize),
    Named(String),
    Finite(bool),
}

#[derive(PartialEq, Debug, Clone)]
pub struct Types(Vec<Option<Type>>);

impl Types {
    pub fn new() -> Types {
        Types(vec![
            Some(Type::Number),
            Some(Type::Bytes(2)),
            Some(Type::Finite(true)),
        ])
    }

    pub fn push(&mut self, it: Type) -> usize {
        if let Some((k, o)) = self.0.iter_mut().enumerate().find(|(_, o)| o.is_none()) {
            *o = Some(it);
            k
        } else {
            self.0.push(Some(it));
            self.0.len() - 1
        }
    }

    pub fn get(&self, at: usize) -> &Type {
        self.0[at].as_ref().unwrap()
    }

    pub fn get_mut(&mut self, at: usize) -> &mut Type {
        self.0[at].as_mut().unwrap()
    }

    pub fn pop(&mut self, at: usize) -> Type {
        let r = self.0[at].take().unwrap();
        if self.0.iter().skip(at).all(|o| o.is_none()) {
            drop(self.0.drain(at..));
        }
        r
    }
}

impl Type {
    pub fn applied(func: usize, give: usize, types: &mut Types) -> Result<usize, Error> {
        let &Type::Func(want, ret) = types.get(func) else {
            return Err(Error::NotFunc(func, types.clone()));
        };

        // want(a, b..) <- give(A, b..)
        Type::concretize(want, give, types)?;

        Ok(ret)
    }

    /// Concretizes boundedness and unknown named types:
    /// * fin <- fin is fin
    /// * inf <- inf is inf
    /// * inf <- fin is fin
    /// * unk <- Knw is Knw
    ///
    /// The others are left unchanged, just checked for assignable.
    /// This uses the fact that inf types and unk named types are
    /// only depended on by the functions that introduced them.
    fn concretize(want: usize, give: usize, types: &mut Types) -> Result<(), Error> {
        match (types.get(want), types.get(give)) {
            (Type::Number, Type::Number) => Ok(()),

            (Type::Bytes(fw), Type::Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Type::Finite(fw_bool), Type::Finite(fg_bool)) if fw_bool == fg_bool => Ok(()),
                (Type::Finite(false), Type::Finite(true)) => {
                    *types.get_mut(*fw) = Type::Finite(true);
                    Ok(())
                }
                (Type::Finite(true), Type::Finite(false)) => Err(Error::InfWhereFinExpected),
                _ => unreachable!(),
            },

            (Type::List(_fw, _), Type::List(_fg, _)) => todo!(),

            (Type::Func(l_arg, l_ret), Type::Func(r_arg, r_ret)) => {
                let (l_arg, r_arg) = (*l_arg, *r_arg);
                let (l_ret, r_ret) = (*l_ret, *r_ret);
                // (a -> b) <- (Str -> Num)
                Type::concretize(l_arg, r_arg, types)?;
                Type::concretize(l_ret, r_ret, types)?;
                Ok(())
            }

            //(Type::Named(_), Type::Named(_)) => todo!(),
            (Type::Named(_), other) => {
                *types.get_mut(want) = other.clone();
                Ok(())
            }

            _ => Err(Error::ExpectedButGot(want, give, types.clone())),
        }
    }

    pub fn repr(&self, types: &Types, out: &mut impl Write) -> Result<(), IoError> {
        match self {
            Type::Number => write!(out, "Num"),

            Type::Bytes(f) => match types.get(*f) {
                Type::Finite(true) => write!(out, "Str"),
                Type::Finite(false) => write!(out, "Str*"),
                _ => unreachable!(),
            },

            Type::List(f, has) => {
                write!(out, "[")?;
                types.get(*has).repr(types, out)?;
                match types.get(*f) {
                    Type::Finite(true) => write!(out, "]"),
                    Type::Finite(false) => write!(out, "]*"),
                    _ => unreachable!(),
                }
            }

            Type::Func(arg, ret) => {
                let funcarg = matches!(types.get(*arg), Type::Func(_, _));
                if funcarg {
                    write!(out, "(")?;
                }
                types.get(*arg).repr(types, out)?;
                if funcarg {
                    write!(out, ")")?;
                }
                write!(out, " -> ")?;
                types.get(*ret).repr(types, out)
            }

            Type::Named(name) => write!(out, "{name}"),

            Type::Finite(true) => write!(out, "??"),
            Type::Finite(false) => write!(out, "??*"),
        }
    }
}
