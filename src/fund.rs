//! fundamental operations

use std::fmt::{Display, Formatter, Result as FmtResult};
use std::rc::Rc;

use crate::types::Type;

/// macro to generate the fn generating the type, internal to `mkmkty!`
macro_rules! mkmktyty {
    ($st:ident, Num) => {
        Type::number()
    };

    ($st:ident, Str+ $f:literal $(& $both:literal)? $(| $either:literal)?) => {
        {
            let f = mkmktyty!($st, + $f $(& $both)? $(| $either)?);
            Type::bytes(f)
        }
    };
    ($st:ident, Str) => {
        mkmktyty!($st, Str+ 0)
    };

    ($st:ident, $n:ident) => {
        $st.$n.clone()
    };

    ($st:ident, [$($i:tt)+]+ $f:literal $(& $both:literal)? $(| $either:literal)?) => {
        {
            let f = mkmktyty!($st, + $f $(& $both)? $(| $either)?);
            let i = mkmktyty!($st, $($i)+);
            Type::list(f, i)
        }
    };
    ($st:ident, [$($i:tt)+]) => {
        mkmktyty!($st, [$($i)+]+ 0)
    };

    ($st:ident, (($($a:tt)+), $($b:tt)+)) => {
        {
            let f = mkmktyty!($st, ($($a)+));
            let s = mkmktyty!($st, $($b)+);
            Type::pair(f, s)
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
            Type::func(p, r)
        }
    };
    ($st:ident, $p:tt+ $f:literal $(& $both:literal)? $(| $either:literal)? -> $($r:tt)+) => {
        mkmktyty!($st, ($p+ $f $(& $both)? $(| $either)?) -> $($r)+)
    };

    ($st:ident, + $n:literal) => {
        false //$st._fin[$n]
    };
    ($st:ident, + $l:literal&$r:literal) => {
        false //$st.types.both($st._fin[$l], $st._fin[$r])
    };
    ($st:ident, + $l:literal|$r:literal) => {
        false //$st.types.either($st._fin[$l], $st._fin[$r])
    };
}

/// macro to generate the fn generating the type: (nb_infinites, names; type)
macro_rules! mkmkty {
    ($f:tt$(, $n:ident)*; $($ty:tt)+) => {
        || {
            struct State {
                //_fin: [Boundedness; $f+1],
                $($n: Rc<Type>,)*
            }
            let _state = State {
                //_fin: mkmkty!(@ types $f),
                $($n: Type::named(stringify!($n).into()),)*
            };
            mkmktyty!(_state, $($ty)+)
        }
    };

    //(@ $types:ident 0) => { [$types.finite(true)] };
    //(@ $types:ident 1) => { [$types.finite(true), $types.finite(false)] };
    //(@ $types:ident 2) => { [$types.finite(true), $types.finite(false), $types.finite(false)] };
    //(@ $types:ident 3) => { [$types.finite(true), $types.finite(false), $types.finite(false), $types.finite(false)] };
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
            pub fn try_from_name(name: &str) -> Option<Self> {
                match name { $($name => Some(Self::$var),)* _ => None }
            }

            pub fn make_type(&self) -> Rc<Type> {
                match self { $(Self::$var => ($mkty as fn() -> Rc<Type>)(),)* }
            }

            pub fn desc(&self) -> &'static [&'static str] {
                match self { $(Self::$var => &[$(stringify!($desc),)+],)+ }
            }
        }
    };
}

make! {
    /// the input
    Stream("-")                  :: mkmkty!(1          ; Str+1                   ),
    /// the type-restricting function
    Astypeof("astypeof")         :: mkmkty!(0, a       ; a -> a -> a             ),
    /// make a list with the head element first, then the rest as tail;
    /// 'snoc lst val' is equivalent to the syntax '{val,, lst}'
    // we use snoc and not cons as it makes better error messages (because it binds the list first)
    Snoc("snoc")                 :: mkmkty!(1, a       ; [a]+1 -> a -> [a]+1     ),
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
