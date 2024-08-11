use phf::{phf_map, Map};

use crate::types::{TypeList, TypeRef};

type BuiltinDesc = (&'static str, fn(&mut TypeList) -> TypeRef);

pub const NAMES: Map<&'static str, BuiltinDesc> = phf_map! {
        "input" => ("the input", |types| {
            // input :: Str+
            let inf = types.infinite();
            types.bytes(inf)
        }),

        "const" => ("todo", |types| {
            // const :: a -> b -> a
            let a = types.named("a");
            let b = types.named("b");
            let ba = types.func(b, a);
            types.func(a, ba)
        }),

        // xxx: maybe change back to nl because of math ln..
        "ln" => ("todo", |types| {
            // ln :: Str+ -> Str+
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            types.func(strinf, strinf)
        }),

        "split" => ("todo", |types| {
            // slice :: Str -> Str+ -> [Str+]+
            let strfin = types.bytes(types.finite());
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            let ret = types.list(inf, strinf);
            let ret = types.func(strinf, ret);
            types.func(strfin, ret)
        }),

        "add" => ("add two numbers", |types| {
            // add :: Num -> Num -> Num
            let ret = types.func(types.number(), types.number());
            types.func(types.number(), ret)
        }),

        "make a new list by applying an unary operation to each value from a list" => ("todo", |types| {
            // map :: (a -> b) -> [a]+ -> [b]+
            let inf = types.infinite();
            let a = types.named("a");
            let b = types.named("b");
            let f = types.func(a, b);
            let aa = types.list(inf, a);
            let bb = types.list(inf, b);
            let ff = types.func(aa, bb);
            types.func(f, ff)
        }),

        "tonum" => ("todo", |types| {
            // tonum :: Str+ -> Num
            let inf = types.infinite();
            let strinf = types.bytes(inf);
            types.func(strinf, types.number())
        }),

        "tostr" => ("todo", |types| {
            // tostr :: Num -> Str
            let strfin = types.bytes(types.finite());
            types.func(types.number(), strfin)
        }),

        "join" => ("todo", |types| {
            // join :: Str -> [Str+]+ -> Str+
            let strfin = types.bytes(types.finite());
            let in_inf1 = types.infinite();
            let in_inf2 = types.infinite();
            let in_strinf = types.bytes(in_inf1);
            let in_lststrinf = types.list(in_inf2, in_strinf);
            let out_inf = types.join(in_inf1, in_inf2);
            let out_strinf = types.bytes(out_inf);
            let ret = types.func(in_lststrinf, out_strinf);
            types.func(strfin, ret)
        }),

        "len" => ("todo", |types| {
            // len :: [a] -> Number
            let a = types.named("a");
            let lstfin = types.list(types.finite(), a);
            types.func(lstfin, types.number())
        }),

        "repeat" => ("todo", |types| {
            // repeat :: a -> [a]+
            let a = types.named("a");
            let inf = types.infinite();
            let lstinf = types.list(inf, a);
            types.func(a, lstinf)
        }),
};
