use crate::parse::{Location, TokenKind};
use crate::types::FrozenType;

#[derive(PartialEq, Debug)]
pub enum ErrorContext {
    Unmatched(TokenKind),
    TypeListInferredItemTypeButItWas(FrozenType, FrozenType),
    // XXX: (both) add 'ButItWas'?
    AsNthArgToNamedNowTyped(usize, String, FrozenType),
    ChainedFromAsNthArgToNamedNowTyped(Location, usize, String, FrozenType),
}

#[derive(PartialEq, Debug)]
pub enum ErrorKind {
    ContextCaused {
        error: Box<Error>,
        because: ErrorContext,
    },
    Unexpected {
        token: TokenKind,
        expected: &'static str,
    },
    UnknownName(String),
    NotFunc(FrozenType),
    ExpectedButGot(FrozenType, FrozenType),
    InfWhereFinExpected,
    FoundTypeHole(FrozenType),
}

#[derive(PartialEq, Debug)]
pub struct Error(pub Location, pub ErrorKind);

#[derive(PartialEq, Debug)]
pub struct ErrorList(pub(crate) Vec<Error>);

impl ErrorList {
    pub fn new() -> ErrorList {
        ErrorList(Vec::new())
    }

    //pub fn len(&self) -> usize {
    //    self.0.len()
    //}

    pub fn push(&mut self, err: Error) {
        self.0.push(err);
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

// crud report eprints {{{
impl Error {
    fn crud_report(&self) {
        use ErrorContext::*;
        use ErrorKind::*;

        match &self.1 {
            ContextCaused { error, because } => {
                error.crud_report();
                match because {
                    Unmatched(token) => eprint!("`-> because of {token:?}"),
                    TypeListInferredItemTypeButItWas(i, o) => {
                        eprint!("`-> because list type was inferred to be [{i}] at this point but this item is {o}")
                    }
                    AsNthArgToNamedNowTyped(nth, name, func) => {
                        eprint!(
                            "`-> because of the parameter in {func} (overall {nth}{} argument to {name})",
                            match nth {
                                1 => "st",
                                2 => "nd",
                                3 => "rd",
                                _ => "th",
                            }
                        );
                    }
                    ChainedFromAsNthArgToNamedNowTyped(from, nth, name, func) => {
                        eprint!(
                            "`-> because of chaining at {from:?} as parameter in {func} (overall {nth}{} argument to {name})",
                            match nth {
                                1 => "st",
                                2 => "nd",
                                3 => "rd",
                                _ => "th",
                            }
                        );
                    }
                }
            }
            Unexpected { token, expected } => {
                eprint!("Unexpected {token:?}, expected {expected}")
            }
            UnknownName(n) => eprint!("Unknown name '{n}'"),
            NotFunc(o) => eprint!("Expected a function type, but got {o}"),
            ExpectedButGot(w, g) => eprint!("Expected type {w}, but got {g}"),
            InfWhereFinExpected => eprint!("Expected finite type, but got infinite type"),
            FoundTypeHole(t) => eprint!("Found type hole, should be {t}"),
        }

        eprintln!(" ==> {:?}", self.0);
    }
}

impl IntoIterator for ErrorList {
    type Item = Error;
    type IntoIter = <Vec<Error> as IntoIterator>::IntoIter;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter()
    }
}

impl ErrorList {
    pub fn crud_report(&self) {
        let ErrorList(errors) = self;
        eprintln!(
            "=== Generated {} error{}",
            errors.len(),
            if 1 == errors.len() { "" } else { "s" }
        );
        for e in errors {
            e.crud_report();
        }
        eprintln!("===");
    }
}
// }}}
