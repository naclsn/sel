use std::fmt::{Display, Formatter, Result as FmtResult};

use crate::parse::{Applicable, TokenKind};
use crate::scope::{Location, SourceRegistry};
use crate::types::FrozenType;

// error types {{{
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
    AsNthArgToNowTyped {
        nth_arg: usize,
        func: Applicable,
        type_with_curr_args: FrozenType,
    },
    ChainedFromAsNthArgToNowTyped {
        comma_loc: Location,
        nth_arg: usize,
        func: Applicable,
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
        func: Applicable,
    },
    ExpectedButGot {
        expected: FrozenType,
        actual: FrozenType,
    },
    InfWhereFinExpected,
    NameAlreadyDeclared {
        name: String,
    },
    CouldNotReadFile {
        error: String,
    },
}

#[derive(PartialEq, Debug)]
pub struct Error(pub Location, pub ErrorKind);

#[derive(Default, PartialEq, Debug)]
pub struct ErrorList(pub(crate) Vec<Error>);

impl IntoIterator for ErrorList {
    type Item = Error;
    type IntoIter = <Vec<Error> as IntoIterator>::IntoIter;

    fn into_iter(self) -> Self::IntoIter {
        self.0.into_iter()
    }
}

impl ErrorList {
    pub(crate) fn push(&mut self, err: Error) {
        self.0.push(err);
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}
// }}}

pub struct Report<'a> {
    registry: &'a SourceRegistry,
    title: String,
    messages: Vec<(Location, String)>,
}

// generate report {{{
impl Error {
    fn ctx_messages(loc: Location, because: &ErrorContext, report: &mut Report) {
        use ErrorContext::*;
        use TokenKind::*;

        let msgs: &[_] = match because {
            Unmatched { open_token } => &[(
                loc,
                format!(
                    "open {} here",
                    match open_token {
                        OpenBracket => '[',
                        OpenBrace => '{',
                        Unknown(_) | Word(_) | Bytes(_) | Number(_) | Comma | CloseBracket
                        | CloseBrace | Equal | Def | Let | Use | Semicolon | End => unreachable!(),
                    }
                ),
            )],
            CompleteType { complete_type } => &[(loc, format!("complete type: {complete_type}"))],
            TypeListInferredItemType { list_item_type } => &[(
                loc,
                format!("list type was inferred to be [{list_item_type}]"),
            )],
            AsNthArgToNowTyped {
                nth_arg,
                func,
                type_with_curr_args,
            } => &[(
                loc,
                format!(
                    "see parameter in {type_with_curr_args} (overall {nth_arg}{} argument to {})",
                    match nth_arg {
                        1 => "st",
                        2 => "nd",
                        3 => "rd",
                        _ => "th",
                    },
                    match func {
                        Applicable::Name(name) => name,
                        Applicable::Bind(_, _, _) => "let binding",
                    },
                ),
            )],
            ChainedFromAsNthArgToNowTyped {
                comma_loc,
                nth_arg,
                func,
                type_with_curr_args,
            } => &[
                (
                    loc,
                    format!(
                        "see parameter in {type_with_curr_args} (overall {nth_arg}{} argument to {})",
                        match nth_arg {
                            1 => "st",
                            2 => "nd",
                            3 => "rd",
                            _ => "th",
                        },
                        match func {
                            Applicable::Name(name) => name,
                            Applicable::Bind(_, _, _) => "let binding",
                        },
                    ),
                ),
                (comma_loc.clone(), "chained through here".into()),
            ],
            ChainedFromToNotFunc { comma_loc } => &[
                (loc, "Not a function".into()),
                (comma_loc.clone(), "chained through here".into()),
            ],
            AutoCoercedVia {
                func_name,
                func_type,
            } => &[(
                loc,
                format!("coerced via '{func_name}' (which has type {func_type})"),
            )],
            DeclaredHereWithType { with_type } => &[(loc, format!("here with type {with_type}"))],
            LetFallbackTypeMismatch {
                result_type,
                fallback_type,
            } => &[(
                loc,
                format!("fallback of type {fallback_type} doesn't match result type {result_type}"),
            )],
        };
        report.messages.extend_from_slice(msgs);
    }

    pub fn report<'a>(&self, registry: &'a SourceRegistry) -> Report<'a> {
        use ErrorKind::*;
        use TokenKind::*;

        let loc = self.0.clone();

        match &self.1 {
            ContextCaused { error, because } => {
                let mut r = error.report(registry);
                Error::ctx_messages(loc, because, &mut r);
                r
            }
            Unexpected { token, expected } => Report {
                registry,
                title: "Unexpected token".into(),
                messages: vec![(
                    loc,
                    format!(
                        "Unexpected '{}', expected {expected}",
                        match token {
                            Unknown(t) => t,
                            Word(v) => v,
                            Bytes(_) => "(bytes)",
                            Number(_) => "(number)",
                            Comma => ",",
                            OpenBracket => "[",
                            CloseBracket => "]",
                            OpenBrace => "{",
                            CloseBrace => "}",
                            Equal => "=",
                            Def => "def",
                            Let => "let",
                            Use => "use",
                            Semicolon => ";",
                            End => "end of script",
                        }
                    ),
                )],
            },
            UnexpectedDefInScript => Report {
                registry,
                title: "Unexpected definition".into(),
                messages: vec![(
                    loc,
                    "these should only appear before the script, at the beginning of the file"
                        .into(),
                )],
            },
            UnknownName {
                name,
                expected_type,
            } => Report {
                registry,
                title: "Unknown name".into(),
                messages: vec![(
                    loc,
                    format!("Unknown name '{name}', should be of type {expected_type}"),
                )],
            },
            NotFunc { actual_type } => Report {
                registry,
                title: "Not a function".into(),
                messages: vec![(
                    loc,
                    format!("Expected a function type, but got {actual_type}"),
                )],
            },
            TooManyArgs { nth_arg, func } => Report {
                registry,
                title: "Too many argument".into(),
                messages: vec![(
                    loc,
                    format!(
                        "Too many argument to {}, expected only {}",
                        match func {
                            Applicable::Name(name) => name,
                            Applicable::Bind(_, _, _) => "let binding",
                        },
                        nth_arg - 1
                    ),
                )],
            },
            ExpectedButGot { expected, actual } => Report {
                registry,
                title: "Type mismatch".into(),
                messages: vec![(loc, format!("Expected type {expected}, but got {actual}"))],
            },
            InfWhereFinExpected => Report {
                registry,
                title: "Wrong boundedness".into(),
                messages: vec![(loc, "Expected finite type, but got infinite type".into())],
            },
            NameAlreadyDeclared { name } => Report {
                registry,
                title: "Name used twice".into(),
                messages: vec![(loc, format!("Name {name} was already declared"))],
            },
            CouldNotReadFile { error } => Report {
                registry,
                title: "Could not read file".into(),
                messages: vec![(loc, format!("<< {error} >>"))],
            },
        }
    }
}
// }}}

// display report {{{
impl Display for Report<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        let (Location(file, ref range), _) = self.messages[0];
        let path = self.registry.get_path(file);
        let bytes = self.registry.get_bytes(file);
        let line = 1 + bytes[range.start..].iter().filter(|c| b'\n' == **c).count();
        writeln!(f, "{}:{line}: {}", path.display(), self.title)?;

        for (_, msg) in &self.messages {
            writeln!(f, " -> {}", msg)?;
        }
        Ok(())
    }
}
// }}}
