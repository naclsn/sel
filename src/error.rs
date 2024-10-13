use std::fmt::{Display, Formatter, Result as FmtResult};

use crate::parse::{Applicable, TokenKind};
use crate::parse::{Token, Tree, TreeKind};
use crate::scope::{Location, ScopeItem, SourceRegistry};
use crate::types::{FrozenType, TypeList, TypeRef};

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
    InconsistentType {
        types: Vec<(Location, FrozenType)>,
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
    use_colors: bool,
}

// error reportig helpers {{{
pub fn context_complete_type(
    types: &TypeList,
    loc: Location,
    err: ErrorKind,
    ty: TypeRef,
) -> ErrorKind {
    let complete_type = types.frozen(ty);
    match &err {
        ErrorKind::ExpectedButGot {
            expected: _,
            actual,
        } if complete_type != *actual => ErrorKind::ContextCaused {
            error: Box::new(Error(loc, err)),
            because: ErrorContext::CompleteType { complete_type },
        },
        _ => err,
    }
}

pub fn context_auto_coerced(
    arg_loc: Location,
    err: ErrorKind,
    func_name: String,
    func_type: FrozenType,
) -> ErrorKind {
    ErrorKind::ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: ErrorContext::AutoCoercedVia {
            func_name,
            func_type,
        },
    }
}

pub fn context_as_nth_arg(
    types: &TypeList,
    arg_loc: Location,
    err: ErrorKind,
    comma_loc: Option<Location>, // if some, ChainedFrom..
    func: &Tree,
) -> ErrorKind {
    let type_with_curr_args = types.frozen(func.ty);
    ErrorKind::ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: {
            // unreachable: func is a function, see `err_context_as_nth_arg` call sites
            let (nth_arg, func) = match &func.value {
                TreeKind::Apply(app, args) => (args.len() + 1, app.clone()),
                _ => unreachable!(),
            };
            match comma_loc {
                Some(comma_loc) => ErrorContext::ChainedFromAsNthArgToNowTyped {
                    comma_loc,
                    nth_arg,
                    func,
                    type_with_curr_args,
                },
                None => ErrorContext::AsNthArgToNowTyped {
                    nth_arg,
                    func,
                    type_with_curr_args,
                },
            }
        },
    }
}

pub fn not_func(types: &TypeList, func: &Tree) -> ErrorKind {
    match &func.value {
        TreeKind::Apply(name, args) => ErrorKind::TooManyArgs {
            nth_arg: args.len() + 1,
            func: name.clone(),
        },
        _ => ErrorKind::NotFunc {
            actual_type: types.frozen(func.ty),
        },
    }
}

pub fn unexpected(token: &Token, expected: &'static str, unmatched: Option<&Token>) -> Error {
    let Token(here, token) = token.clone();
    let mut err = Error(here, ErrorKind::Unexpected { token, expected });
    if let Some(unmatched) = unmatched {
        let Token(from, open_token) = unmatched.clone();
        err = Error(
            from,
            ErrorKind::ContextCaused {
                error: Box::new(err),
                because: ErrorContext::Unmatched { open_token },
            },
        );
    }
    err
}

pub fn list_type_mismatch(
    types: &TypeList,
    item_loc: Location,
    err: ErrorKind,
    item_type: TypeRef,
    open_loc: Location,
    list_item_type: TypeRef,
) -> Error {
    Error(
        open_loc,
        ErrorKind::ContextCaused {
            error: Box::new(Error(
                item_loc.clone(),
                context_complete_type(types, item_loc, err, item_type),
            )),
            because: ErrorContext::TypeListInferredItemType {
                list_item_type: types.frozen(list_item_type),
            },
        },
    )
}

pub fn already_declared(
    types: &TypeList,
    name: String,
    new_loc: Location,
    previous: &ScopeItem,
) -> Error {
    Error(
        previous
            .get_loc()
            .cloned()
            .unwrap_or_else(|| new_loc.clone()),
        ErrorKind::ContextCaused {
            error: Box::new(Error(new_loc, ErrorKind::NameAlreadyDeclared { name })),
            because: ErrorContext::DeclaredHereWithType {
                with_type: match previous {
                    ScopeItem::Builtin(mkty, _) => {
                        let mut types = TypeList::default();
                        let ty = mkty(&mut types);
                        types.frozen(ty)
                    }
                    ScopeItem::Defined(val, _) => types.frozen(val.ty),
                    ScopeItem::Binding(_, ty) => types.frozen(*ty),
                },
            },
        },
    )
}
// }}}

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
                        | CloseBrace | Equal | Def | Let | Use | End => unreachable!(),
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
            DeclaredHereWithType { with_type } => &[(loc, format!("declared here with type {with_type}"))],
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

    pub fn report<'a>(&self, registry: &'a SourceRegistry, use_colors: bool) -> Report<'a> {
        use ErrorKind::*;
        use TokenKind::*;

        let loc = self.0.clone();

        match &self.1 {
            ContextCaused { error, because } => {
                let mut r = error.report(registry, use_colors);
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
                            Def => "keyword def",
                            Let => "keyword let",
                            Use => "keyword use",
                            End => "end of script",
                        }
                    ),
                )],
                use_colors,
            },
            UnknownName {
                name,
                expected_type,
            } => Report {
                registry,
                title: "Unknown name".into(),
                messages: vec![(
                    loc,
                    // TODO: (could) search in scope for similar names and for matching types
                    format!("Unknown name '{name}', should be of type {expected_type}"),
                )],
                use_colors,
            },
            NotFunc { actual_type } => Report {
                registry,
                title: "Not a function".into(),
                messages: vec![(
                    loc,
                    format!("Expected a function type, but got {actual_type}"),
                )],
                use_colors,
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
                use_colors,
            },
            ExpectedButGot { expected, actual } => Report {
                registry,
                title: "Type mismatch".into(),
                // TODO: (could) if expression of type `actual` is a name, search in scope,
                //               otherwise search for function like `actual -> expected`
                messages: vec![(loc, format!("Expected type {expected}, but got {actual}"))],
                use_colors,
            },
            InfWhereFinExpected => Report {
                registry,
                title: "Wrong boundedness".into(),
                // TODO: (could) recommend eg `take` if applicable? -> note: this goes toward
                // making recommendations based on actually written vs guessed intent; there is
                // a whole thing to studdy here which would sit in between parser and error
                messages: vec![(loc, "Expected finite type, but got infinite type".into())],
                use_colors,
            },
            NameAlreadyDeclared { name } => Report {
                registry,
                title: "Name used twice".into(),
                messages: vec![(loc, format!("Name {name} was already declared"))],
                use_colors,
            },
            CouldNotReadFile { error } => Report {
                registry,
                title: "Could not read file".into(),
                messages: vec![(loc, format!("<< {error} >>"))],
                use_colors,
            },
            InconsistentType { types } => Report {
                registry,
                title: "Inconsistent type from usage".into(),
                messages: types
                    .iter()
                    .map(|(loc, ty)| (loc.clone(), format!("Use here with type {ty}")))
                    .collect(),
                use_colors,
            },
        }
    }
}
// }}}

// display report {{{
impl Display for Report<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        let ctop: &str = if self.use_colors { "\x1b[33m" } else { "" };
        let cmsg: &str = if self.use_colors { "\x1b[34m" } else { "" };
        let carr: &str = if self.use_colors { "\x1b[35m" } else { "" };
        let cnum: &str = if self.use_colors { "\x1b[36m" } else { "" };
        let r: &str = if self.use_colors { "\x1b[m" } else { "" };

        let (Location(file, range), _) = &self.messages[0];
        let source = self.registry.get(*file);

        let lnum = source.get_containing_lnum(range.start).unwrap();
        writeln!(
            f,
            "{}:{lnum}: {ctop}{}{r}",
            source.path.display(),
            self.title
        )?;

        for (Location(file, range), msg) in &self.messages {
            let source = &self.registry.get(*file);
            let (first_lnum, lranges) = source.get_containing_lines(range).unwrap();

            if let [lrange, _, ..] = lranges {
                writeln!(
                    f,
                    "{ctop}|{r}        {}{carr}.{}{r}",
                    " ".repeat(range.start - lrange.start),
                    "-".repeat(lrange.end - range.start - 1)
                )?;
            }

            for (lnum, lrange) in lranges.iter().enumerate() {
                writeln!(
                    f,
                    "{ctop}|{cnum}{:5} {carr}|{r} {}",
                    lnum + first_lnum,
                    String::from_utf8_lossy(&source.bytes[lrange.clone()])
                )?;
            }

            match lranges {
                [lrange] => write!(
                    f,
                    "{ctop}|{r}        {}{carr}{}{r}",
                    " ".repeat(range.start - lrange.start),
                    "-".repeat(range.len())
                )?,
                [.., lrange] => write!(
                    f,
                    "{ctop}|{r}        {carr}{}'{r}",
                    "-".repeat(range.end - lrange.start - 1)
                )?,
                _ => unreachable!("empty ranges"),
            }

            writeln!(f, " {cmsg}{msg}{r}")?;
        }

        writeln!(f, "{ctop}==={r}")
    }
}
// }}}
