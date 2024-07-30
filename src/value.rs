use crate::reform::{ATree, Error}; // XXX: dont like it

#[derive(PartialEq, Debug, Clone, Copy)]
pub enum Boundedness {
    Finite,
    Infinite,
}

#[derive(PartialEq, Debug, Clone)]
pub enum Type {
    Number,
    Bytes(Boundedness),
    List(Boundedness, usize),
    Func(usize, usize),
    Named(String),
}

#[derive(PartialEq, Debug)]
pub enum Value {
    Number(i32),
    Bytes(Vec<u8>),
    List(Vec<ATree>),
    Func(String),
}

impl Type {
    pub fn applied(func: usize, give: usize, types: &mut[Type]) -> Result<usize, Error> {
        let Type::Func(want, ret) = types[func] else {
             return Err(Error::NotFunc(func, types.to_vec()));
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
    fn concretize(want: usize, give: usize, types: &mut[Type]) -> Result<(), Error> {
        match (&types[want], &types[give]) {
            (Type::Number, Type::Number) => Ok(()),
            (Type::Bytes(Boundedness::Infinite), Type::Bytes(f)) => {
                types[want] = Type::Bytes(*f);
                Ok(())
            },
            (Type::Bytes(Boundedness::Finite), Type::Bytes(Boundedness::Finite)) => Ok(()),

            (Type::List(Boundedness::Infinite, _), Type::List(_f, _)) => todo!(),
            (Type::List(Boundedness::Finite, _), Type::List(Boundedness::Finite, _)) => todo!(),

            (Type::Func(l_arg, l_ret), Type::Func(r_arg, r_ret)) => {
                let (l_arg, r_arg) = (*l_arg, *r_arg);
                let (l_ret, r_ret) = (*l_ret, *r_ret);
                // (a -> b) <- (Str -> Num)
                Type::concretize(l_arg, r_arg, types)?;
                Type::concretize(l_ret, r_ret, types)?;
                Ok(())
            },

            //(Type::Named(_), Type::Named(_)) => todo!(),
            (Type::Named(_), other) => {
                types[want] = other.clone();
                Ok(())
            },

            (_, _) => Err(Error::ExpectedButGot(want, give, types.to_vec())),
        }
    }

    pub fn repr(&self, types: &[Type], out: &mut impl std::io::Write) -> Result<(), std::io::Error> {
        match self {
            Type::Number => write!(out, "Num"),
            Type::Bytes(Boundedness::Finite) => write!(out, "Str"),
            Type::Bytes(Boundedness::Infinite) => write!(out, "Str*"),

            Type::List(f, has) => {
                write!(out, "[")?;
                types[*has].repr(types, out)?;
                match f {
                    Boundedness::Finite => write!(out, "]"),
                    Boundedness::Infinite => write!(out, "]*"),
                }
            },

            Type::Func(arg, ret) => {
                let funcarg = matches!(types[*arg], Type::Func(_, _));
                if funcarg { write!(out, "(")?; }
                types[*arg].repr(types, out)?;
                if funcarg { write!(out, ")")?; }
                write!(out, " -> ")?;
                types[*ret].repr(types, out)
            },
            Type::Named(name) => write!(out, "{name}"),
        }
    }
}
