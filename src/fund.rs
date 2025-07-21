//! fundamental operations

use std::fmt::{Display, Formatter, Result as FmtResult};

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

macro_rules! make {
    ($($(#[$desc:meta])+ $var:ident($name:literal) :: $mkty:expr),*$(,)?) => {
        /// Fundamental operations that cannot be implemented in the language itself.
        ///
        /// This does not mean everything else can be done within the language;
        /// eg OS-level interractions are to be implemented somewhere else. Likewise,
        /// not all maths may be accessible with that limited of a set (idk really).
        ///
        /// It basically represent what an interpreter will in any way need to implement.
        /// Technically `pipe`, `tonum` and `tostr` _could_ be implemented in the prelude
        /// but are not for consistency with other syntax-level functions.
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        pub enum Fund { $($(#[$desc])+ $var,)* }

        impl Display for Fund {
            fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
                write!(f, "{}", match self { $(Self::$var => $name,)* })
            }
        }

        impl Fund {
            const NAMES: &[&str] = &[$($name,)*];

            pub fn try_from_name(name: &str) -> Option<Self> {
                match name { $($name => Some(Self::$var),)* _ => None }
            }

            pub fn make_type(&self, types: &mut TypeList) -> TypeRef {
                match self { $(Self::$var => ($mkty as fn(&mut TypeList) -> TypeRef)(types),)* }
            }

            pub fn desc(&self) -> &'static [&'static str] {
                match self { $(Self::$var => &[$(stringify!($desc),)+],)+ }
            }
        }
    };
}
// }}}

make! {
    /// the input
    Stream("-")                  :: mkmkty!(1          ; Str+1                   ),
    /// make a list with the head element first, then the rest as tail;
    /// 'cons val lst' is equivalent to the syntax '{val,, lst}'
    Cons("cons")                 :: mkmkty!(1, a       ; a -> [a]+1 -> [a]+1     ),
    /// panics (runtime abort with message)
    Panic("panic")               :: mkmkty!(0, a       ; Str -> a                ),
    /// pipe two functions;
    /// 'pipe one two' is equivalent to the syntax 'one, two' ie 'two(one(..))'
    /// (see also 'compose')
    Pipe("pipe")                 :: mkmkty!(0, a, b, c ; (a->b) -> (b->c) -> a->c),
    /// convert a string into number;
    /// accept an infinite string for convenience but stop on the first invalid byte
    Tonum("tonum")               :: mkmkty!(1          ; Str+1 -> Num            ),
    /// convert a number into string
    Tostr("tostr")               :: mkmkty!(0          ; Num -> Str              ),

    /// make a list of numbers with the 8 bits bytes
    Bytes("bytes")               :: mkmkty!(1          ; Str+1 -> [Num]+1        ),
    /// make a list of numbers with the 32 bits codepoints
    Codepoints("codepoints")     :: mkmkty!(1          ; Str+1 -> [Num]+1        ),
    /// make a list of strings with the potentially multi-codepoints graphemes
    Graphemes("graphemes")       :: mkmkty!(1          ; Str+1 -> [Str]+1        ),
    /// make a string from the 8 bits bytes
    Unbytes("unbytes")           :: mkmkty!(1          ; [Num]+1 -> Str+1        ),
    /// make a string from the 32 bits codepoints
    Uncodepoints("uncodepoints") :: mkmkty!(1          ; [Num]+1 -> Str+1        ),
    /// make a string from the potentially multi-codepoints graphemes
    Ungraphemes("ungraphemes")   :: mkmkty!(1          ; [Str]+1 -> Str+1        ),

    /// add two numbers
    Add("add")                   :: mkmkty!(0          ; Num -> Num -> Num       ),
    /// invert a number
    Invert("invert")             :: mkmkty!(0          ; Num -> Num              ),
    /// multiply two numbers
    Mul("mul")                   :: mkmkty!(0          ; Num -> Num -> Num       ),
    /// negate a number
    Negate("negate")             :: mkmkty!(0          ; Num -> Num              ),
    /// sign of a number, in {-1, 0, 1}
    Signum("signum")             :: mkmkty!(0          ; Num -> Num              ),
    /// truncation (rounding toward zero)
    Trunc("trunc")               :: mkmkty!(0          ; Num -> Num              ),

    /// the inverse sine function
    Asin("asin")                 :: mkmkty!(0          ; Num -> Num              ),
    /// the exponent function
    Exp("exp")                   :: mkmkty!(0          ; Num -> Num              ),
    /// the natural logarithm function
    Log("log")                   :: mkmkty!(0          ; Num -> Num              ),
    /// the sine function
    Sin("sin")                   :: mkmkty!(0          ; Num -> Num              ),
}
