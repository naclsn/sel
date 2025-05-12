// TODO(wip): trim down, rename to fundamentals or something
use crate::scope::{Scope, ScopeItem};
use crate::types::{Boundedness, TypeList, TypeRef};

// macro to generate the fn generating the type {{{
#[macro_export]
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

    ($st:ident, (($($a:tt)+), $($b:tt)+)) => {
        {
            let f = mkmktyty!($st, ($($a)+));
            let s = mkmktyty!($st, $($b)+);
            $st.types.pair(f, s)
        }
    };
    ($st:ident, ($a:tt, $($b:tt)+)) => {
        mkmktyty!($st, (($a), $($b)+))
    };
    ($st:ident, ($a:tt+ $f:literal $(& $both:literal)? $(| $either:literal)?, $($b:tt)+)) => {
        mkmktyty!($st, (($a+ $f $(& $both)? $(| $either)?), $($b)+))
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
#[macro_export]
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
                $($n: types.named(stringify!($n).into()),)*
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

macro_rules! mkbin {
    ($mkty:expr, $desc:literal) => {
        ScopeItem::Builtin($mkty, $desc)
    };
}
// }}}

pub fn scope() -> Scope {
    let mut r = Scope::default();
    for (k, v) in NAMES {
        r.declare(k.into(), v);
    }
    r
}

/// The fundamental operations that cannot be implemented in the language itself.
/// It basically represent what an interpreter will in any way need to implement.
/// Technically `pipe`, `tonum` and `tostr` _could_ be implemented in the prelude
/// but are not for consistency with other coercion-like functions.
const NAMES: [(&str, ScopeItem); 16] = [
    ("-"            , mkbin!(mkmkty!(1          ; Str+1                   ), "the input")),
    ("bytes"        , mkbin!(mkmkty!(1          ; Str+1 -> [Num]+1        ), "make a list of numbers with the 8 bits bytes")),
    ("codepoints"   , mkbin!(mkmkty!(1          ; Str+1 -> [Num]+1        ), "make a list of numbers with the 32 bits codepoints")),
    ("cons"         , mkbin!(mkmkty!(1, a       ; a -> [a]+1 -> [a]+1     ), "make a list with the head element first, then the rest as tail; 'cons val lst' is equivalent to the syntax '{val,, lst}'")),
    ("graphemes"    , mkbin!(mkmkty!(1          ; Str+1 -> [Str]+1        ), "make a list of strings with the potentially multi-codepoints graphemes")),
    ("panic"        , mkbin!(mkmkty!(0, a       ; Str -> a                ), "panics; this is different from fatal: fatal is a parse-time abort, panic is a runtime abort")),
    ("pipe"         , mkbin!(mkmkty!(0, a, b, c ; (a->b) -> (b->c) -> a->c), "pipe two function; 'pipe one two' is equivalent to the syntax 'one, two' ie 'two(one(..))' (see also 'compose')")),
    ("tonum"        , mkbin!(mkmkty!(1          ; Str+1 -> Num            ), "convert a string into number; accept an infinite string for convenience but stop on the first invalid byte")),
    ("tostr"        , mkbin!(mkmkty!(0          ; Num -> Str              ), "convert a number into string")),
    ("unbytes"      , mkbin!(mkmkty!(1          ; [Num]+1 -> Str+1        ), "make a string from the 8 bits bytes")),
    ("uncodepoints" , mkbin!(mkmkty!(1          ; [Num]+1 -> Str+1        ), "make a string from the 32 bits codepoints")),
    ("ungraphemes"  , mkbin!(mkmkty!(1          ; [Str]+1 -> Str+1        ), "make a string from the potentially multi-codepoints graphemes")),

    ("add"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num       ), "add two numbers")),
    ("mul"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num       ), "multiply two numbers")),
    // that or negate :: Num -> Num
    ("sub"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num       ), "substract the second numbers --or the other way around? idk")),
    // that or invert :: Num -> Num
    ("div"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num       ), "divide by the second numbers --or the other way around? idk")),
    // floor/ceil
    // pow/exp
    // ln/log
];

/*
    ("add"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num                          ), "add two numbers")),
    ("div"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num                          ), "divide by the second numbers")),

    ("join"         , mkbin!(mkmkty!(2          ; Str -> [Str+1]+2 -> Str+1&2                ), "join a list of string with a separator between entries")),
    ("split"        , mkbin!(mkmkty!(1          ; Str -> Str+1 -> [Str+1]+1                  ), "break a string into pieces separated by the argument, consuming the delimiter; note that an empty delimiter does not split the input on every character, but rather is equivalent to `const [repeat ::]`, for this see `graphemes`")),

    ("init"         , mkbin!(mkmkty!(0, a       ; [a] -> [a]                                 ), "extract the initial part (until the last item)")),
    ("last"         , mkbin!(mkmkty!(0, a       ; [a] -> a                                   ), "extract the last item")),
    ("lookup"       , mkbin!(mkmkty!(0, a       ; [(Str, a)] -> Str -> [a]                   ), "lookup in the list of key/value pairs the value associated with the given key; the return is an option (ie a singleton or an empty list)")),
    ("map"          , mkbin!(mkmkty!(1, a, b    ; (a -> b) -> [a]+1 -> [b]+1                 ), "make a new list by applying an unary operation to each value from a list")),
    ("repeat"       , mkbin!(mkmkty!(1, a       ; a -> [a]+1                                 ), "repeat an infinite amount of copies of the same value")),
    ("take"         , mkbin!(mkmkty!(1, a       ; Num -> [a]+1 -> [a]                        ), "take only the first items of a list, or fewer")),
    ("zipwith"      , mkbin!(mkmkty!(2, a, b, c ; (a -> b -> c) -> [a]+1 -> [b]+2 -> [c]+1|2 ), "make a new list by applying an binary operation to each corresponding value from each lists; stops when either list ends")),
*/
