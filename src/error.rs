use crate::parse::{Location, TokenKind};
use crate::types::FrozenType;

#[derive(PartialEq, Debug)]
pub enum ErrorContext {
    Unmatched(TokenKind),
    TypeListInferredItemTypeButItWas {
        list_item_type: FrozenType,
        new_item_type: FrozenType,
    },
    // XXX: (both) add 'ButItWas'?
    AsNthArgToNamedNowTyped {
        nth_arg: usize,
        func_name: String,
        type_with_curr_args: FrozenType,
    },
    ChainedFromAsNthArgToNamedNowTyped {
        comma_loc: Location,
        nth_arg: usize,
        func_name: String,
        type_with_curr_args: FrozenType,
    },
    ChainedFromToNotFunc {
        comma_loc: Location,
    },
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
    UnknownName {
        name: String,
        expected_type: FrozenType,
    },
    NotFunc {
        actual_type: FrozenType,
    },
    TooManyArgs {
        nth_arg: usize,
        func_name: String,
    },
    ExpectedButGot {
        expected: FrozenType,
        actual: FrozenType,
    },
    InfWhereFinExpected,
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
                    TypeListInferredItemTypeButItWas {
                        list_item_type,
                        new_item_type,
                    } => {
                        eprint!("`-> because list type was inferred to be [{list_item_type}] at this point but this item is {new_item_type}")
                    }
                    AsNthArgToNamedNowTyped {
                        nth_arg,
                        func_name,
                        type_with_curr_args,
                    } => {
                        eprint!(
                            "`-> because of the parameter in {type_with_curr_args} (overall {nth_arg}{} argument to {func_name})",
                            match nth_arg {
                                1 => "st",
                                2 => "nd",
                                3 => "rd",
                                _ => "th",
                            }
                        );
                    }
                    ChainedFromAsNthArgToNamedNowTyped {
                        comma_loc,
                        nth_arg,
                        func_name,
                        type_with_curr_args,
                    } => {
                        eprint!(
                            "`-> because of chaining at {comma_loc:?} as parameter in {type_with_curr_args} (overall {nth_arg}{} argument to {func_name})",
                            match nth_arg {
                                1 => "st",
                                2 => "nd",
                                3 => "rd",
                                _ => "th",
                            }
                        );
                    }
                    ChainedFromToNotFunc { comma_loc } => {
                        eprint!("`-> because of chaining at {comma_loc:?}")
                    }
                }
            }
            Unexpected { token, expected } => {
                eprint!("Unexpected {token:?}, expected {expected}")
            }
            UnknownName {
                name,
                expected_type,
            } => eprint!("Unknown name '{name}', should be {expected_type}"),
            NotFunc { actual_type } => eprint!("Expected a function type, but got {actual_type}"),
            TooManyArgs { nth_arg, func_name } => {
                eprint!(
                    "Too many argument to {func_name}, expected only {}",
                    nth_arg - 1
                )
            }
            ExpectedButGot { expected, actual } => {
                eprint!("Expected type {expected}, but got {actual}")
            }
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
