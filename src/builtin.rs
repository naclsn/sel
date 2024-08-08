use crate::parse::{Type, TypeList, TypeRef, FINITE_TYPEREF, NUMBER_TYPEREF, STRFIN_TYPEREF};

pub fn lookup_type(name: &str, types: &mut TypeList) -> Option<TypeRef> {
    match name {
        "input" => {
            // input :: Str*
            let inf = types.push(Type::Finite(false));
            Some(types.push(Type::Bytes(inf)))
        }

        "const" => {
            let a = types.push(Type::Named("a".into()));
            let b = types.push(Type::Named("b".into()));
            let ba = types.push(Type::Func(b, a));
            Some(types.push(Type::Func(a, ba)))
        }

        // xxx: maybe change to nl because of math ln..
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
            let nton = types.push(Type::Func(NUMBER_TYPEREF, NUMBER_TYPEREF));
            Some(types.push(Type::Func(NUMBER_TYPEREF, nton)))
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
            Some(types.push(Type::Func(strinf, NUMBER_TYPEREF)))
        }

        "tostr" => {
            // tostr :: Num -> Str
            Some(types.push(Type::Func(NUMBER_TYPEREF, STRFIN_TYPEREF)))
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
            Some(types.push(Type::Func(STRFIN_TYPEREF, blablablabla)))
        }

        "len" => {
            // len :: [a] -> Number
            let a = types.push(Type::Named("a".into()));
            let lst = types.push(Type::List(FINITE_TYPEREF, a));
            Some(types.push(Type::Func(lst, NUMBER_TYPEREF)))
        }

        _ => None,
    }
}
