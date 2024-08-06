use crate::parse::{Type, TypeList, TypeRef};

pub fn lookup_type(name: &str, types: &mut TypeList) -> Option<TypeRef> {
    match name {
        "(,)" => {
            // (,) :: (a -> b) -> (b -> c) -> a -> c
            let a = types.push(Type::Named("a".into()));
            let b = types.push(Type::Named("b".into()));
            let c = types.push(Type::Named("c".into()));
            let ab = types.push(Type::Func(a, b));
            let bc = types.push(Type::Func(b, c));
            let ac = types.push(Type::Func(a, c));
            let push = types.push(Type::Func(bc, ac));
            Some(types.push(Type::Func(ab, push)))
        }

        "input" => {
            // input :: Str*
            let inf = types.push(Type::Finite(false));
            Some(types.push(Type::Bytes(inf)))
        }

        "ln" => {
            // ln :: Str* -> Str*
            let inf = types.push(Type::Finite(false));
            let strinf = types.push(Type::Bytes(inf));
            Some(types.push(Type::Func(strinf, strinf)))
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

        "map" => {
            // map :: (a -> b) -> [a]* -> [b]*
            let inf = types.push(Type::Finite(false));
            let a = types.push(Type::Named("a".into()));
            let b = types.push(Type::Named("b".into()));
            let aa = types.push(Type::List(inf, a));
            let bb = types.push(Type::List(inf, b));
            let f = types.push(Type::Func(a, b));
            let ff = types.push(Type::Func(aa, bb));
            Some(types.push(Type::Func(f, ff)))
        }

        "tonum" => {
            // tonum :: Str* -> Num
            let inf = types.push(Type::Finite(false));
            let strinf = types.push(Type::Bytes(inf));
            Some(types.push(Type::Func(strinf, 0)))
        }

        "tostr" => {
            // tostr :: Num -> Str
            Some(types.push(Type::Func(0, 1)))
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
