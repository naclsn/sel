use std::fs::File;
use std::io::{Read, Result as IoResult};
use std::ops::Range;
use std::path::{Path, PathBuf};

use ariadne::{ColorGenerator, Label, Report, ReportBuilder, ReportKind};

use crate::parse::{Applicable, TokenKind};
use crate::types::FrozenType;

// source registry {{{
pub type SourceRef = usize;

#[derive(PartialEq, Debug, Clone)]
pub struct Location(pub SourceRef, pub Range<usize>);

pub struct SourceRegistry(Vec<(PathBuf, Option<Vec<u8>>)>);

impl SourceRegistry {
    pub fn new() -> SourceRegistry {
        SourceRegistry(Vec::new())
    }

    /// Note: the given path is taken as is (ie not canonicalize) and not checked for duplication
    pub fn add_bytes(&mut self, name: impl AsRef<Path>, bytes: Vec<u8>) -> SourceRef {
        self.0.push((name.as_ref().to_owned(), Some(bytes)));
        self.0.len() - 1
    }

    pub fn add(&mut self, name: impl AsRef<Path>) -> IoResult<SourceRef> {
        let name = name.as_ref().canonicalize()?;
        if let Some(found) = self.0.iter().position(|c| c.0 == name) {
            return Ok(found);
        }
        let mut bytes = Vec::new();
        File::open(&name)?.read_to_end(&mut bytes)?;
        Ok(self.add_bytes(name, bytes))
    }

    pub fn get_name(&self, at: SourceRef) -> &Path {
        &self.0[at].0
    }

    pub fn get_bytes(&self, at: SourceRef) -> &[u8] {
        self.0[at].1.as_ref().unwrap()
    }

    pub fn take_bytes(&mut self, at: SourceRef) -> Vec<u8> {
        self.0[at].1.take().unwrap()
    }

    pub fn put_back_bytes(&mut self, at: SourceRef, bytes: Vec<u8>) {
        self.0[at].1 = Some(bytes);
    }
}
// }}}

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
    pub(crate) fn new() -> ErrorList {
        ErrorList(Vec::new())
    }

    pub(crate) fn push(&mut self, err: Error) {
        self.0.push(err);
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
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
            Report::build(ReportKind::Error, (), self.0 .1.start)
        };
        let l = Label::new(self.0 .1.clone()).with_color(colors.next());

        match &self.1 {
            ContextCaused { error: _, because } => match because {
                Unmatched { open_token } => r.with_label(l.with_message(format!(
                    "open {} here",
                    match open_token {
                        OpenBracket => '[',
                        OpenBrace => '{',
                        Unknown(_) | Word(_) | Bytes(_) | Number(_) | Comma | CloseBracket
                        | CloseBrace | Equal | Def | Let | Use | Semicolon | End => unreachable!(),
                    }
                ))),
                CompleteType { complete_type } => {
                    r.with_label(l.with_message(format!("complete type: {complete_type}")))
                }
                TypeListInferredItemType { list_item_type } => r.with_label(
                    l.with_message(format!("list type was inferred to be [{list_item_type}]")),
                ),
                AsNthArgToNowTyped {
                    nth_arg,
                    func,
                    type_with_curr_args,
                } => r.with_label(l.with_message(format!(
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
                ))),
                ChainedFromAsNthArgToNowTyped {
                    comma_loc,
                    nth_arg,
                    func,
                    type_with_curr_args,
                } => r
                    .with_label(l.with_message(format!(
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
                    )))
                    .with_label(
                        Label::new(comma_loc.1.clone())
                            .with_color(colors.next())
                            .with_message("chained through here"),
                    ),
                ChainedFromToNotFunc { comma_loc } => {
                    r.with_label(l.with_message("Not a function")).with_label(
                        Label::new(comma_loc.1.clone())
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
                l.with_message(format!("Unknown name '{name}', should be of type {expected_type}")),
            ),
            NotFunc { actual_type } => r.with_message("Not a function").with_label(
                l.with_message(format!("Expected a function type, but got {actual_type}")),
            ),
            TooManyArgs { nth_arg, func } => {
                r.with_message("Too many argument")
                    .with_label(l.with_message(format!(
                        "Too many argument to {}, expected only {}",
                        match func {
                            Applicable::Name(name) => name,
                            Applicable::Bind(_, _, _) => "let binding",
                        },
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
            CouldNotReadFile { error } => r
                .with_message("Could not read file")
                .with_label(l.with_message(format!("<< {error} >>"))),
        }
    }

    pub fn pretty(&self) -> Report {
        self.get_builder(&mut ColorGenerator::new()).finish()
    }
}
