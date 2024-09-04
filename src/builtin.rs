use crate::scope::{Scope, ScopeItem};
use crate::types::{Boundedness, TypeList, TypeRef};

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
    let mut r = Scope::new(None);
    for (k, v) in NAMES {
        r.declare(k.into(), v);
    }
    r
}

const NAMES: [(&str, ScopeItem); 40] = [
    ("-"            , mkbin!(mkmkty!(1          ; Str+1                                      ), "the input")),
    ("add"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num                          ), "add two numbers")),
    ("apply"        , mkbin!(mkmkty!(0, a, b    ; (a -> b) -> a -> b                         ), "apply argument to function; 'apply f x' is equivalent to 'f x'")),
    ("bytes"        , mkbin!(mkmkty!(1          ; Str+1 -> [Num]+1                           ), "make a list of numbers with the 8 bits bytes that makes the bytestream")),
    ("codepoints"   , mkbin!(mkmkty!(1          ; Str+1 -> [Num]+1                           ), "make a list of numbers with the 32 bits codepoints")),
    ("compose"      , mkbin!(mkmkty!(0, a, b, c ; (b -> c) -> (a -> b) -> a -> c             ), "compose two function; 'compose one two' is equivalent to the syntax 'two, one' ie 'one(two(..))' (see also 'pipe')")),
    ("const"        , mkbin!(mkmkty!(0, a, b    ; a -> b -> a                                ), "always evaluate to its first argument, ignoring its second argument")),
    ("curry"        , mkbin!(mkmkty!(0, a, b, c ; ((a, b) -> c) -> a -> b -> c               ), "curry")),
    ("div"          , mkbin!(mkmkty!(0          ; Num -> Num -> Num                          ), "divide by the second numbers")),
    ("duple"        , mkbin!(mkmkty!(0, a       ; a -> (a, a)                                ), "fundamentally THE operation that makes the types system non-linear!")),
    ("empty"        , mkbin!(mkmkty!(0, a       ; [a]                                        ), "equivalent to {}")), // | "none"
    ("enumerate"    , mkbin!(mkmkty!(1, a       ; [a]+1 -> [(Num, a)]+1                      ), "enumerate a list into a list of (index, value)")),
    ("flip"         , mkbin!(mkmkty!(0, a, b, c ; (a -> b -> c) -> b -> a -> c               ), "flip the order in which the parameter are needed")),
    ("fold"         , mkbin!(mkmkty!(0, a, b    ; (b -> a -> b) -> b -> [a] -> b             ), "")),
    ("fst"          , mkbin!(mkmkty!(0, a, b    ; (a, b) -> a                                ), "first")), // | "first" | "car"
    ("graphemes"    , mkbin!(mkmkty!(1          ; Str+1 -> [Str]+1                           ), "make a list of strings with the potentially multi-codepoints graphemes")),
    ("head"         , mkbin!(mkmkty!(1, a       ; [a]+1 -> a                                 ), "extract the head (the first item)")), // | "unwrap"
    ("init"         , mkbin!(mkmkty!(0, a       ; [a] -> [a]                                 ), "extract the initial part (until the last item)")),
    ("iterate"      , mkbin!(mkmkty!(1, a       ; (a -> a) -> a -> [a]+1                     ), "create an infinite list where the first item is calculated by applying the function on the second argument, the second item by applying the function on the previous result and so on")),
    ("join"         , mkbin!(mkmkty!(2          ; Str -> [Str+1]+2 -> Str+1&2                ), "join a list of string with a separator between entries")),
    ("last"         , mkbin!(mkmkty!(0, a       ; [a] -> a                                   ), "extract the last item")),
    ("len"          , mkbin!(mkmkty!(0, a       ; [a] -> Num                                 ), "compute the length of a finite list")), // | "issome"
    ("ln"           , mkbin!(mkmkty!(1          ; Str+1 -> Str+1                             ), "append a new line to a string (xxx: maybe change back to nl because of math ln..)")),
    ("lookup"       , mkbin!(mkmkty!(0, a       ; [(Str, a)] -> Str -> [a]                   ), "lookup in the list of key/value pairs the value associated with the given key; the return is an option (ie a singleton or an empty list)")),
    ("map"          , mkbin!(mkmkty!(1, a, b    ; (a -> b) -> [a]+1 -> [b]+1                 ), "make a new list by applying an unary operation to each value from a list")),
    ("pair"         , mkbin!(mkmkty!(0, a, b    ; a -> b -> (a, b)                           ), "make a pair")),
    ("pipe"         , mkbin!(mkmkty!(0, a, b, c ; (a -> b) -> (b -> c) -> a -> c             ), "pipe two function; 'pipe one two' is equivalent to the syntax 'one, two' ie 'two(one(..))' (see also 'compose')")),
    ("repeat"       , mkbin!(mkmkty!(1, a       ; a -> [a]+1                                 ), "repeat an infinite amount of copies of the same value")),
    ("singleton"    , mkbin!(mkmkty!(0, a       ; a -> [a]                                   ), "equivalent to {a}")), // | "once" | "just" | "some"
    ("snd"          , mkbin!(mkmkty!(0, a, b    ; (a, b) -> b                                ), "second")), // | "second" | "cdr"
    ("split"        , mkbin!(mkmkty!(1          ; Str -> Str+1 -> [Str+1]+1                  ), "break a string into pieces separated by the argument, consuming the delimiter; note that an empty delimiter does not split the input on every character, but rather is equivalent to `const [repeat ::]`, for this see `graphemes`")),
    ("tail"         , mkbin!(mkmkty!(1, a       ; [a]+1 -> [a]+1                             ), "extract the tail (past the first item)")),
    ("take"         , mkbin!(mkmkty!(1, a       ; Num -> [a]+1 -> [a]                        ), "take only the first items of a list, or fewer")),
    ("tonum"        , mkbin!(mkmkty!(1          ; Str+1 -> Num                               ), "convert a string into number, accept an infinite string for convenience but stop on the first byte that is not in '0'..='9'")),
    ("tostr"        , mkbin!(mkmkty!(0          ; Num -> Str                                 ), "convert a number into string")),
    ("uncodepoints" , mkbin!(mkmkty!(1          ; [Num]+1 -> Str+1                           ), "make a list of numbers with the 32 bits codepoints")),
    ("uncurry"      , mkbin!(mkmkty!(0, a, b, c ; (a -> b -> c) -> (a, b) -> c               ), "uncurry")),
    ("ungraphemes"  , mkbin!(mkmkty!(1          ; [Str]+1 -> Str+1                           ), "make a list of strings with the potentially multi-codepoints graphemes")),
    ("unpair"       , mkbin!(mkmkty!(0, a, b, c ; (a, b) -> (a -> b -> c) -> c               ), "unmake a pair")),
    ("zipwith"      , mkbin!(mkmkty!(2, a, b, c ; (a -> b -> c) -> [a]+1 -> [b]+2 -> [c]+1|2 ), "make a new list by applying an binary operation to each corresponding value from each lists; stops when either list ends")),
];
