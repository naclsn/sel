use crate::parse::{Type, TypeList, TypeRef};

pub fn lookup_type(name: &str, types: &mut TypeList) -> Option<TypeRef> {
    match name {
        "input" => {
            let inf = types.push(Type::Finite(false));
            Some(types.push(Type::Bytes(inf)))
        }

        "split" => {
            // slice :: Str -> Str* -> [Str*]*
            let inf = types.push(Type::Finite(false));
            let strinf = types.push(Type::Bytes(inf));
            let lststrinf = types.push(Type::List(inf, strinf));
            let blablablabla = types.push(Type::Func(strinf, lststrinf));
            Some(types.push(Type::Func(1, blablablabla)))
        }

        "add" => {
            // add :: Num -> Num -> Num
            let nton = types.push(Type::Func(0, 0));
            Some(types.push(Type::Func(0, nton)))
        }

        "join" => {
            // join :: Str -> [Str*]* -> Str*
            // uuhh :<
            // this is still not enough, both * in [Str*]*
            // should be &&'d when applying the 2nd arg
            let inf = types.push(Type::Finite(false));
            let strinf = types.push(Type::Bytes(inf));
            let lststrinf = types.push(Type::List(inf, strinf));
            let blablablabla = types.push(Type::Func(lststrinf, strinf));
            Some(types.push(Type::Func(1, blablablabla)))
        }

        _ => None,
    }
}
