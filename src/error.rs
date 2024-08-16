use std::fmt::{Result as FmtResult, Write};
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
                        eprint!("`-> because list type was inferred to be [{list_item_type}] at this point")
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

// json report {{{
struct JsonQuotingWriter<'a, W: Write>(&'a mut W);
impl<W: Write> Write for JsonQuotingWriter<'_, W> {
    fn write_str(&mut self, mut s: &str) -> FmtResult {
        while let Some(k) = s.find(|c| matches!(c, '"' | '\\' | '/' | 'b' | 'f' | 'n' | 'r' | 't'))
        {
            self.0.write_str(&s[..k])?;
            self.0.write_char('\\')?;
            self.0.write_str(&s[k..k + 1])?;
            s = &s[k + 1..];
        }
        self.0.write_str(s)
    }
}

impl Error {
    pub fn json(&self, w: &mut impl Write) -> FmtResult {
        use ErrorContext::*;
        use ErrorKind::*;
        use TokenKind::*;

        write!(w, "[{},", self.0 .0)?;
        match &self.1 {
            ContextCaused { error, because } => {
                write!(w, "\"ContextCaused\",{{")?;
                write!(w, "\"error\":")?;
                error.json(w)?;
                write!(w, ",")?;
                write!(w, "\"because\":[")?;
                match because {
                    Unmatched { open_token } => {
                        write!(w, "\"Unmatched\",{{")?;
                        write!(
                            w,
                            "\"open_token\":\"{}\"",
                            match open_token {
                                OpenBracket => '[',
                                OpenBrace => '{',
                                Unknown(_) | Word(_) | Bytes(_) | Number(_) | Comma
                                | CloseBracket | CloseBrace | End => unreachable!(),
                            }
                        )?;
                        write!(w, "}}")?;
                    }
                    CompleteType { complete_type } => {
                        write!(w, "\"CompleteType\",{{")?;
                        write!(w, "\"complete_type\":\"")?;
                        write!(JsonQuotingWriter(w), "{complete_type}")?;
                        write!(w, "\"")?;
                        write!(w, "}}")?;
                    }
                    TypeListInferredItemType { list_item_type } => {
                        write!(w, "\"TypeListInferredItemType\",{{")?;
                        write!(w, "\"list_item_type\":\"")?;
                        write!(JsonQuotingWriter(w), "{list_item_type}")?;
                        write!(w, "\"")?;
                        write!(w, "}}")?;
                    }
                    AsNthArgToNamedNowTyped {
                        nth_arg,
                        func_name,
                        type_with_curr_args,
                    } => {
                        write!(w, "\"AsNthArgToNamedNowTyped\",{{")?;
                        write!(w, "\"nth_arg\":{nth_arg},")?;
                        write!(w, "\"func_name\":\"")?;
                        write!(JsonQuotingWriter(w), "{func_name}")?;
                        write!(w, "\",")?;
                        write!(w, "\"type_with_curr_args\":\"")?;
                        write!(JsonQuotingWriter(w), "{type_with_curr_args}")?;
                        write!(w, "\"")?;
                        write!(w, "}}")?;
                    }
                    ChainedFromAsNthArgToNamedNowTyped {
                        comma_loc,
                        nth_arg,
                        func_name,
                        type_with_curr_args,
                    } => {
                        write!(w, "\"ChainedFromAsNthArgToNamedNowTyped\",{{")?;
                        write!(w, "\"comma_loc\":{},", comma_loc.0)?;
                        write!(w, "\"nth_arg\":{nth_arg},")?;
                        write!(w, "\"func_name\":\"")?;
                        write!(JsonQuotingWriter(w), "{func_name}")?;
                        write!(w, "\",")?;
                        write!(w, "\"type_with_curr_args\":\"")?;
                        write!(JsonQuotingWriter(w), "{type_with_curr_args}")?;
                        write!(w, "\"")?;
                        write!(w, "}}")?;
                    }
                    ChainedFromToNotFunc { comma_loc } => {
                        write!(w, "\"ChainedFromToNotFunc\",{{")?;
                        write!(w, "\"comma_loc\":{}", comma_loc.0)?;
                        write!(w, "}}")?;
                    }
                }
                write!(w, "]}}")?;
            }
            Unexpected { token, expected } => {
                write!(w, "\"Unexpected\",{{")?;
                write!(w, "\"token\":\"")?;
                match token {
                    Unknown(t) => write!(JsonQuotingWriter(w), "{t}")?,
                    Comma => write!(w, ",")?,
                    CloseBracket => write!(w, "]")?,
                    CloseBrace => write!(w, "}}")?,
                    End => (),
                    Word(_) | Bytes(_) | Number(_) | OpenBracket | OpenBrace => unreachable!(),
                }
                write!(w, "\",")?;
                write!(w, "\"expected\":\"{expected}\"")?;
                write!(w, "}}")?;
            }
            UnknownName {
                name,
                expected_type,
            } => {
                write!(w, "\"UnknownName\",{{")?;
                write!(w, "\"name\":\"")?;
                write!(JsonQuotingWriter(w), "{name}")?;
                write!(w, "\",")?;
                write!(w, "\"expected_type\":\"")?;
                write!(JsonQuotingWriter(w), "{expected_type}")?;
                write!(w, "\"")?;
                write!(w, "}}")?;
            }
            NotFunc { actual_type } => {
                write!(w, "\"NotFunc\",{{")?;
                write!(w, "\"actual_type\":\"")?;
                write!(JsonQuotingWriter(w), "{actual_type}")?;
                write!(w, "\"")?;
                write!(w, "}}")?;
            }
            TooManyArgs { nth_arg, func_name } => {
                write!(w, "\"TooManyArgs\",{{")?;
                write!(w, "\"nth_arg\":{nth_arg},")?;
                write!(w, "\"func_name\":\"")?;
                write!(JsonQuotingWriter(w), "{func_name}")?;
                write!(w, "\"")?;
                write!(w, "}}")?;
            }
            ExpectedButGot { expected, actual } => {
                write!(w, "\"ExpectedButGot\",{{")?;
                write!(w, "\"expected\":\"")?;
                write!(JsonQuotingWriter(w), "{expected}")?;
                write!(w, "\",")?;
                write!(w, "\"actual\":\"")?;
                write!(JsonQuotingWriter(w), "{actual}")?;
                write!(w, "\"")?;
                write!(w, "}}")?;
            }
            InfWhereFinExpected => write!(w, "\"InfWhereFinExpected\",{{}}")?,
        }
        write!(w, "]")
    }
}

impl ErrorList {
    pub fn json(&self, w: &mut impl Write) -> FmtResult {
        write!(w, "[")?;
        let mut sep = "";
        for e in &self.0 {
            write!(w, "{sep}")?;
            sep = ",";
            e.json(w)?;
        }
        write!(w, "]")
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
                        | CloseBrace | End => unreachable!(),
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
                    "see parameter in {type_with_curr_args} (overall {nth_arg}{} argument to {func_name})",
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
                } => {
                    r.with_label(l.with_message(format!(
                        "see parameter in {type_with_curr_args} (overall {nth_arg}{} argument to {func_name})",
                        match nth_arg {
                            1 => "st",
                            2 => "nd",
                            3 => "rd",
                            _ => "th",
                        }
                    ))).with_label(
                        Label::new(comma_loc.0..comma_loc.0+1)
                            .with_color(colors.next())
                            .with_message("chained through here")
                    )
                }
                ChainedFromToNotFunc { comma_loc } => {
                        r.with_label(l.with_message("Not a function")).with_label(Label::new(comma_loc.0..comma_loc.0+1).with_color(colors.next()).with_message("chained through here"))
                }
            }
            Unexpected { token, expected } => {
                r.with_message("Unexpected token").with_label(l.with_message(format!("Unexpected {}, expected {expected}", match token {
                    Unknown(t) => t,
                    Comma => ",",
                    CloseBracket => "]",
                    CloseBrace => "}",
                    End => "end of script",
                    Word(_) | Bytes(_) | Number(_) | OpenBracket | OpenBrace => unreachable!(),

                })))
            }
            UnknownName {
                name,
                expected_type,
            } => r.with_message("Unknown name").with_label(l.with_message(format!("Unknown name '{name}', should be {expected_type}"))),
            NotFunc { actual_type } => {
                r.with_message("Not a function").with_label(l.with_message(format!("Expected a function type, but got {actual_type}")))
            }
            TooManyArgs { nth_arg, func_name } => r.with_message("Too many argument").with_label(l.with_message(format!(
                "Too many argument to {func_name}, expected only {}",
                nth_arg - 1
            ))),
            ExpectedButGot { expected, actual } => {
                r.with_message("Wrong type").with_label(l.with_message(format!("Expected type {expected}, but got {actual}")))
            }
            InfWhereFinExpected => {
                r.with_message("Wrong type").with_label(l.with_message("Expected finite type, but got infinite type"))
            }
        }
    }

    pub fn pretty(&self) -> Report {
        self.get_builder(&mut ColorGenerator::new()).finish()
    }
}
