use phf::{phf_map, Map};

use crate::types::{Boundedness, TypeList, TypeRef};

type BuiltinDesc = (fn(&mut TypeList) -> TypeRef, &'static str);

// macro to generate the fn generating the type {{{
macro_rules! mkmktyty {
    ($st:ident, Num) => {
        $st.types.number()
    };

    ($st:ident, Str+ $f:literal $(& $both:literal)? $(| $either:literal)?) => {
        {
            let f = mkmktyty!($st, + $f $(& $both)? $(| $either)?);
            $st.types.bytes(f)
        }
    };
    ($st:ident, Str) => {
        mkmktyty!($st, Str+ 0)
    };

    ($st:ident, $n:ident) => {
        $st.$n
    };

    ($st:ident, [$($i:tt)+]+ $f:literal $(& $both:literal)? $(| $either:literal)?) => {
        {
            let f = mkmktyty!($st, + $f $(& $both)? $(| $either)?);
            let i = mkmktyty!($st, $($i)+);
            $st.types.list(f, i)
        }
    };
    ($st:ident, [$($i:tt)+]) => {
        mkmktyty!($st, [$($i)+]+ 0)
    };

    ($st:ident, ($($t:tt)+)) => {
        mkmktyty!($st, $($t)+)
    };

    ($st:ident, $p:tt -> $($r:tt)+) => {
        {
            let p = mkmktyty!($st, $p);
            let r = mkmktyty!($st, $($r)+);
            $st.types.func(p, r)
        }
    };
    ($st:ident, $p:tt+ $f:literal $(& $both:literal)? $(| $either:literal)? -> $($r:tt)+) => {
        mkmktyty!($st, ($p+ $f $(& $both)? $(| $either)?) -> $($r)+)
    };

    ($st:ident, + $n:literal) => {
        $st._fin[$n]
    };
    ($st:ident, + $l:literal&$r:literal) => {
        $st.types.both($st._fin[$l], $st._fin[$r])
    };
    ($st:ident, + $l:literal|$r:literal) => {
        $st.types.either($st._fin[$l], $st._fin[$r])
    };
}

// (nb_infinites, names; type)
macro_rules! mkmkty {
    ($f:tt$(, $n:ident)*; $($ty:tt)+) => {
        |types| {
            struct State<'a> {
                _fin: [Boundedness; $f+1],
                $($n: TypeRef,)*
                types: &'a mut TypeList,
            }
            let state = State {
                _fin: mkmkty!(@ types $f),
                $($n: types.named(stringify!($n)),)*
                types,
            };
            mkmktyty!(state, $($ty)+)
        }
    };

    (@ $types:ident 0) => { [$types.finite(true)] };
    (@ $types:ident 1) => { [$types.finite(true), $types.finite(false)] };
    (@ $types:ident 2) => { [$types.finite(true), $types.finite(false), $types.finite(false)] };
    (@ $types:ident 3) => { [$types.finite(true), $types.finite(false), $types.finite(false), $types.finite(false)] };
}
// }}}

pub const NAMES: Map<&'static str, BuiltinDesc> = phf_map! {
    "add" => (mkmkty!(0; Num -> Num -> Num),
        "add two numbers"),

    "const" => (mkmkty!(0, a, b; a -> b -> a),
        "always evaluate to its first argument, ignoring its second argument"),

    "input" => (mkmkty!(1; Str+1),
        "the input"),

    "join" => (mkmkty!(2; Str -> [Str+1]+2 -> Str+1&2),
        "join a list of string with a separator between entries"),

    "len" => (mkmkty!(0, a; [a] -> Num),
        "compute the length of a finite list"),

    "ln" => (mkmkty!(1; Str+1 -> Str+1),
        "append a new line to a string (xxx: maybe change back to nl because of math ln..)"),

    "map" => (mkmkty!(1, a, b; (a -> b) -> [a]+1 -> [b]+1),
        "make a new list by applying an unary operation to each value from a list"),

    "repeat" => (mkmkty!(1, a; a -> [a]+1),
        "repeat an infinite amount of copies of the same value"),

    "split" => (mkmkty!(1; Str -> Str+1 -> [Str+1]+1),
        "break a string into pieces separated by the argument, consuming the delimiter; note that an empty delimiter does not split the input on every character, but rather is equivalent to `const [repeat ::]`, for this see `graphemes`"),

    "tonum" => (mkmkty!(1; Str+1 -> Num),
        "convert a string into number"),

    "tostr" => (mkmkty!(0; Num -> Str),
        "convert a number into string"),

    "zipwith" => (mkmkty!(2, a, b, c; (a -> b -> c) -> [a]+1 -> [b]+2 -> [c]+1|2),
        "make a new list by applying an binary operation to each corresponding value from each lists; stops when either list ends"),
};
