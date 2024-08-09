use crate::parse::Token;
use crate::types::{TypeList, TypeRef};

#[derive(PartialEq, Debug)]
pub enum ErrorContext {
    Unmatched(Token /* just the location */), // WIP
    TypeListInferred(TypeRef),                // WIP
    //ArgComesFrom... or something like this
}

#[derive(PartialEq, Debug)]
pub enum Error {
    ContextCaused {
        error: Box<Error>,
        because: ErrorContext,
    },
    Unexpected {
        token: Token,
        expected: &'static str,
    },
    UnexpectedEnd,
    UnknownName(String),
    NotFunc(TypeRef),
    ExpectedButGot(TypeRef, TypeRef),
    InfWhereFinExpected,
}

#[derive(PartialEq, Debug)]
pub struct ErrorList(pub Vec<Error>, pub TypeList);

impl Error {
    fn crud_report(&self, types: &TypeList) {
        match self {
            Error::ContextCaused { error, because } => {
                error.crud_report(types);
                match because {
                    ErrorContext::Unmatched(token) => eprintln!("Because of {token:?}"),
                    &ErrorContext::TypeListInferred(i) => eprintln!(
                        "Because list type was inferred to be {} at this point",
                        types.repr(i)
                    ),
                }
            }
            Error::Unexpected { token, expected } => {
                eprintln!("Unexpected {token:?}, expected {expected}")
            }
            Error::UnexpectedEnd => eprintln!("Unexpected end of script"),
            Error::UnknownName(n) => eprintln!("Unknown name '{n}'"),
            &Error::NotFunc(o) => {
                eprintln!("Expected a function type, but got {}", types.repr(o))
            }
            &Error::ExpectedButGot(w, g) => {
                eprintln!("Expected type {}, but got {}", types.repr(w), types.repr(g))
            }
            Error::InfWhereFinExpected => {
                eprintln!("Expected finite type, but got infinite type")
            }
        }
    }
}

impl ErrorList {
    pub fn crud_report(&self) {
        let ErrorList(errors, types) = self;
        eprintln!(
            "=== Generated {} error{}",
            errors.len(),
            if 1 == errors.len() { "" } else { "s" }
        );
        for e in errors {
            e.crud_report(types);
        }
        eprintln!("===");
    }
}
