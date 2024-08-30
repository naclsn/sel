use std::ops::Range;

use ariadne::{ColorGenerator, Label, Report, ReportBuilder, ReportKind};

use crate::parse::{Location, TokenKind};
use crate::types::FrozenType;

#[derive(PartialEq, Debug)]
pub enum ErrorContext {
    Unmatched {
        open_token: TokenKind,
    },
    CompleteType {
        complete_type: FrozenType,
    },
    TypeListInferredItemType {
        list_item_type: FrozenType,
    },
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
    AutoCoercedVia {
        func_name: String,
        func_type: FrozenType,
    },
    DeclaredHereWithType {
        with_type: FrozenType,
    },
    LetFallbackTypeMismatch {
        result_type: FrozenType,
        fallback_type: FrozenType,
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
    UnexpectedDefInScript,
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
    NameAlreadyDeclared {
        name: String,
    },
}

#[derive(PartialEq, Debug)]
pub struct Error(pub Location, pub ErrorKind);

#[derive(PartialEq, Debug)]
pub struct ErrorList(pub(crate) Vec<Error>);

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

    pub fn push(&mut self, err: Error) {
        self.0.push(err);
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

// crud report eprints {{{
impl Error {
    #[allow(dead_code)]
    fn crud_report(&self) {
        use ErrorContext::*;
        use ErrorKind::*;

        match &self.1 {
            ContextCaused { error, because } => {
                error.crud_report();
                match because {
                    Unmatched { open_token } => eprint!("`-> because of {open_token:?}"),
                    CompleteType { complete_type } => eprint!("`-> complete type: {complete_type}"),
                    TypeListInferredItemType { list_item_type } => {
                        eprint!("`-> because list type was inferred to be [{list_item_type}]");
                    }
                    AsNthArgToNamedNowTyped {
                        nth_arg,
                        func_name,
                        type_with_curr_args,
                    } => {
                        eprint!("`-> because of the parameter in ");
                        eprint!(
                            "{type_with_curr_args} (overall {nth_arg}{} argument to {func_name})",
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
                        eprint!("`-> because of chaining at {comma_loc:?} as parameter in ");
                        eprint!(
                            "{type_with_curr_args} (overall {nth_arg}{} argument to {func_name})",
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
                    AutoCoercedVia {
                        func_name,
                        func_type,
                    } => eprint!("`-> with auto coercion via {func_name} :: {func_type}"),
                    DeclaredHereWithType { with_type } => {
                        eprint!("`-> declared here as {with_type}")
                    }
                    LetFallbackTypeMismatch {
                        result_type,
                        fallback_type,
                    } => eprint!(
                        "`-> fallback type {fallback_type} doesn't match result type {result_type}"
                    ),
                }
            }
            Unexpected { token, expected } => {
                eprint!("Unexpected {token:?}, expected {expected}")
            }
            UnexpectedDefInScript => eprint!("Unexpected definition within script"),
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
            NameAlreadyDeclared { name } => eprint!("Name {name} was already declared"),
        }

        eprintln!(" ==> {:?}", self.0);
    }
}

impl ErrorList {
    #[allow(dead_code)]
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

impl Error {
    fn get_builder(&self, colors: &mut ColorGenerator) -> ReportBuilder<Range<usize>> {
        use ErrorContext::*;
        use ErrorKind::*;
        use TokenKind::*;

        let r = if let ContextCaused { error, because: _ } = &self.1 {
            error.get_builder(colors)
        } else {
            Report::build(ReportKind::Error, (), self.0 .0)
        };
        let l = Label::new(self.0 .0..self.0 .0 + 1).with_color(colors.next());

        match &self.1 {
            ContextCaused { error: _, because } => match because {
                Unmatched { open_token } => r.with_label(l.with_message(format!(
                    "open {} here",
                    match open_token {
                        OpenBracket => '[',
                        OpenBrace => '{',
                        Unknown(_) | Word(_) | Bytes(_) | Number(_) | Comma | CloseBracket
                        | CloseBrace | Equal | Def | Let | Semicolon | End => unreachable!(),
                    }
                ))),
                CompleteType { complete_type } => {
                    r.with_label(l.with_message(format!("complete type: {complete_type}")))
                }
                TypeListInferredItemType { list_item_type } => r.with_label(
                    l.with_message(format!("list type was inferred to be [{list_item_type}]")),
                ),
                AsNthArgToNamedNowTyped {
                    nth_arg,
                    func_name,
                    type_with_curr_args,
                } => r.with_label(l.with_message(format!(
                    "see parameter in {type_with_curr_args} (overall {nth_arg}{} argument to \
                     {func_name})",
                    match nth_arg {
                        1 => "st",
                        2 => "nd",
                        3 => "rd",
                        _ => "th",
                    }
                ))),
                ChainedFromAsNthArgToNamedNowTyped {
                    comma_loc,
                    nth_arg,
                    func_name,
                    type_with_curr_args,
                } => r
                    .with_label(l.with_message(format!(
                        "see parameter in {type_with_curr_args} (overall {nth_arg}{} argument to \
                         {func_name})",
                        match nth_arg {
                            1 => "st",
                            2 => "nd",
                            3 => "rd",
                            _ => "th",
                        }
                    )))
                    .with_label(
                        Label::new(comma_loc.0..comma_loc.0 + 1)
                            .with_color(colors.next())
                            .with_message("chained through here"),
                    ),
                ChainedFromToNotFunc { comma_loc } => {
                    r.with_label(l.with_message("Not a function")).with_label(
                        Label::new(comma_loc.0..comma_loc.0 + 1)
                            .with_color(colors.next())
                            .with_message("chained through here"),
                    )
                }
                AutoCoercedVia {
                    func_name,
                    func_type,
                } => r.with_label(l.with_message(format!(
                    "coerced via '{func_name}' (which has type {func_type})"
                ))),
                DeclaredHereWithType { with_type } => {
                    r.with_label(l.with_message(format!("here with type {with_type}")))
                }
                LetFallbackTypeMismatch {
                    result_type,
                    fallback_type,
                } => r.with_label(l.with_message(format!(
                    "fallback of type {fallback_type} doesn't match result type {result_type}"
                ))),
            },
            Unexpected { token, expected } => {
                r.with_message("Unexpected token")
                    .with_label(l.with_message(format!(
                        "Unexpected '{}', expected {expected}",
                        match token {
                            Unknown(t) => t,
                            Word(v) => v, // actually unreachable
                            Bytes(_) => ":...:",
                            Number(_) => "(number)",
                            Comma => ",",
                            OpenBracket => "[",
                            CloseBracket => "]",
                            OpenBrace => "{",
                            CloseBrace => "}",
                            Equal => "=",
                            Def => "def",
                            Let => "let",
                            Semicolon => ";",
                            End => "end of script",
                        }
                    )))
            }
            UnexpectedDefInScript => {
                r.with_message("Unexpected definition")
                    .with_label(l.with_message(
                        "these should only appear before the script, at the beginning of the file",
                    ))
            }
            UnknownName {
                name,
                expected_type,
            } => r.with_message("Unknown name").with_label(
                l.with_message(format!("Unknown name '{name}', should be {expected_type}")),
            ),
            NotFunc { actual_type } => r.with_message("Not a function").with_label(
                l.with_message(format!("Expected a function type, but got {actual_type}")),
            ),
            TooManyArgs { nth_arg, func_name } => {
                r.with_message("Too many argument")
                    .with_label(l.with_message(format!(
                        "Too many argument to {func_name}, expected only {}",
                        nth_arg - 1
                    )))
            }
            ExpectedButGot { expected, actual } => r
                .with_message("Type mismatch")
                .with_label(l.with_message(format!("Expected type {expected}, but got {actual}"))),
            InfWhereFinExpected => r
                .with_message("Wrong boundedness")
                .with_label(l.with_message("Expected finite type, but got infinite type")),
            NameAlreadyDeclared { name } => r
                .with_message("Name used twice")
                .with_label(l.with_message(format!("Name {name} was already declared"))),
        }
    }

    pub fn pretty(&self) -> Report {
        self.get_builder(&mut ColorGenerator::new()).finish()
    }
}
