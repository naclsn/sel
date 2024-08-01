use crate::parse::Error;
use crate::typing::{Type, Types};

pub fn lookup_type(name: &str, types: &mut Types) -> Result<usize, Error> {
    match name {
        "input" => {
            let inf = types.push(Type::Finite(false));
            Ok(types.push(Type::Bytes(inf)))
        }
        "split" => {
            // slice :: Str -> Str* -> [Str*]*
            let inf = types.push(Type::Finite(false));
            let strinf = types.push(Type::Bytes(inf));
            let lststrinf = types.push(Type::List(inf, strinf));
            let blablablabla = types.push(Type::Func(strinf, lststrinf));
            Ok(types.push(Type::Func(1, blablablabla)))
        }
        "join" => {
            // join :: Str -> [Str*]* -> Str*
            // uuhh :<
            todo!()
        }
        _ => Err(Error::UnknownName(name.to_string())),
    }
}
