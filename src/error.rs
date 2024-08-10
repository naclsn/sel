use crate::parse::{Location, TokenKind};
use crate::types::FrozenType;

#[derive(PartialEq, Debug)]
pub enum ErrorContext {
    Unmatched(TokenKind),          // WIP
    TypeListInferred(FrozenType),  // WIP
    AsNthArgTo(usize, FrozenType), // WIP
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
}

#[derive(PartialEq, Debug)]
pub struct Error(pub Location, pub ErrorKind);

#[derive(PartialEq, Debug)]
pub struct ErrorList(Vec<Error>);

impl Error {
    fn crud_report(&self) {
        use ErrorContext::*;
        use ErrorKind::*;

        match &self.1 {
            ContextCaused { error, because } => {
                error.crud_report();
                match because {
                    Unmatched(token) => eprint!("`-> because of {token:?}"),
                    TypeListInferred(i) => {
                        eprint!("`-> because list type was inferred to be {i} at this point")
                    }
                    AsNthArgTo(n, par) => eprint!("`-> because of the {n} parameter at {par:?}"),
                }
            }

            Unexpected { token, expected } => {
                eprint!("Unexpected {token:?}, expected {expected}")
            }

            UnknownName(n) => eprint!("Unknown name '{n}'"),

            NotFunc(o) => eprint!("Expected a function type, but got {o}"),
            ExpectedButGot(w, g) => eprint!("Expected type {w}, but got {g}"),
            InfWhereFinExpected => eprint!("Expected finite type, but got infinite type"),
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
