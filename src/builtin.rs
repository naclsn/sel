use crate::types::{TypeList, TypeRef};

pub fn lookup_type(name: &str, types: &mut TypeList) -> Option<TypeRef> {
    Some(match name {
        "input" => {
            // input :: Str*
            let inf = types.infinite();
            types.bytes(inf)
        }

        "const" => {
            // const :: a -> b -> a
            let a = types.named("a");
            let b = types.named("b");
            let ba = types.func(b, a);
            types.func(a, ba)
        }

        // xxx: maybe change back to nl because of math ln..
        "ln" => {
            // ln :: Str* -> Str*
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            types.func(strinf, strinf)
        }

        "split" => {
            // slice :: Str -> Str* -> [Str*]*
            let strfin = types.bytes(types.finite());
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            let ret = types.list(inf, strinf);
            let ret = types.func(strinf, ret);
            types.func(strfin, ret)
        }

        "add" => {
            // add :: Num -> Num -> Num
            let ret = types.func(types.number(), types.number());
            types.func(types.number(), ret)
        }

        "map" => {
            // map :: (a -> b) -> [a]* -> [b]*
            let inf = types.infinite();
            let a = types.named("a");
            let b = types.named("b");
            let f = types.func(a, b);
            let aa = types.list(inf, a);
            let bb = types.list(inf, b);
            let ff = types.func(aa, bb);
            types.func(f, ff)
        }

        "tonum" => {
            // tonum :: Str* -> Num
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            types.func(strinf, types.number())
        }

        "tostr" => {
            // tostr :: Num -> Str
            let strfin = types.bytes(types.finite());
            types.func(types.number(), strfin)
        }

        "join" => {
            // join :: Str -> [Str*]* -> Str*
            // uuhh :<
            // this is still not enough, both * in [Str*]*
            // should be &&'d when applying the 2nd arg
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            let lststrinf = types.list(inf, strinf);
            let strfin = types.bytes(types.finite());
            let ret = types.func(lststrinf, strinf);
            types.func(strfin, ret)
        }

        "len" => {
            // len :: [a] -> Number
            let a = types.named("a");
            let lstfin = types.list(types.finite(), a);
            types.func(lstfin, types.number())
        }

        "repeat" => {
            // repeat :: a -> [a]*
            let a = types.named("a");
            let inf = types.infinite();
            let lstinf = types.list(inf, a);
            types.func(a, lstinf)
        }

        _ => return None,
    })
}
