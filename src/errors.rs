//! error structs, helpers and reporting

use std::fmt::{Display, Formatter, Result as FmtResult};
use std::io::IsTerminal;
use std::rc::Rc;

use crate::check::{Refers, TreeVal};
use crate::lex::{Token, TokenKind};
use crate::module::{Location, ModuleRegistry};
use crate::parse::{ApplyBase, Located};
use crate::types::Type;

// error types {{{
#[derive(Debug)]
pub enum ErrorContext {
    AsNthArgToNowTyped {
        nth_arg: usize,
        func: ApplyBase,
        type_with_curr_args: Rc<Type>,
    },
    AutoCoercedVia {
        func_name: String,
        func_type: Rc<Type>,
    },
    ChainedFromAsNthArgToNowTyped {
        comma_loc: Location,
        nth_arg: usize,
        func: ApplyBase,
        type_with_curr_args: Rc<Type>,
    },
    ChainedFromToNotFunc {
        comma_loc: Location,
    },
    CompleteType {
        complete_type: Rc<Type>,
    },
    DeclaredHereWithType {
        with_type: Rc<Type>,
    },
    FuncTooManyArgs {
        nth_arg: usize,
        func: String,
    },
    LetAlreadyApplied,
    LetFallbackRequired,
    LetFallbackTypeMismatch {
        result_type: Rc<Type>,
        fallback_type: Rc<Type>,
    },
    ListExtraCommaMakesRest,
    ListTypeInferredItemType {
        list_item_type: Rc<Type>,
    },
    Unmatched {
        open_token: TokenKind,
    },
}

#[derive(Debug)]
pub enum ErrorKind {
    ContextCaused {
        error: Box<Error>,
        because: ErrorContext,
    },
    CouldNotReadFile {
        error: String,
    },
    ExpectedButGot {
        expected: Rc<Type>,
        actual: Rc<Type>,
    },
    InconsistentType {
        types: Vec<(Location, Rc<Type>)>,
    },
    InfWhereFinExpected,
    NameAlreadyDeclared {
        name: String,
    },
    NotFunc {
        actual_type: Rc<Type>,
    },
    Unexpected {
        token: TokenKind,
        expected: &'static str,
    },
    UnknownName {
        name: String,
        expected_type: Rc<Type>,
    },
    Utf8Error {
        text: Box<[u8]>,
        error: std::str::Utf8Error,
    },
}

use ErrorContext::*;
use ErrorKind::*;

#[derive(Debug)]
pub struct Error(Location, ErrorKind);
// }}}

pub struct Report<'a> {
    registry: &'a ModuleRegistry,
    title: String,
    messages: Vec<(Location, String)>,
}

// error reportig helpers {{{
pub fn broken_utf8(loc: Location, text: Box<[u8]>, error: std::str::Utf8Error) -> Error {
    Error(loc, Utf8Error { text, error })
}

pub fn unexpected(token: Token, expected: &'static str, unmatched: Option<Token>) -> Error {
    let Token(here, token) = token;
    let err = Error(here, Unexpected { token, expected });
    if let Some(Token(from, open_token)) = unmatched {
        Error(
            from,
            ContextCaused {
                error: Box::new(err),
                because: Unmatched { open_token },
            },
        )
    } else {
        err
    }
}

pub fn unknown_name(loc: Location, name: String, ty: &Type) -> Error {
    // TODO: these will have to be done at very last, and thus lose order
    Error(
        loc,
        UnknownName {
            name,
            expected_type: ty.deep_clone(),
        },
    )
}

pub fn not_function(
    loc_sad: Location,
    ty: &Type,
    maybe_func_with_too_many_args: &TreeVal,
) -> Error {
    let err = Error(
        loc_sad,
        NotFunc {
            actual_type: ty.deep_clone(),
        },
    );

    match maybe_func_with_too_many_args {
        TreeVal::Apply(base, args) => match &base.val {
            TreeVal::Word(name, prov) => Error(
                match prov {
                    Refers::Fundamental(_fund) => err.0.clone(),
                    Refers::Binding(loc) => loc.clone(),
                    Refers::Defined(func) => func.loc.clone(),
                    Refers::File(_module, func) => func.loc.clone(),
                    Refers::Missing => return err,
                },
                ContextCaused {
                    error: err.into(),
                    because: FuncTooManyArgs {
                        nth_arg: args.len() + 1,
                        func: name.clone(),
                    },
                },
            ),

            TreeVal::Binding(pat, _, _) => Error(
                pat.loc(),
                ContextCaused {
                    error: err.into(),
                    because: LetAlreadyApplied,
                },
            ),

            _ => err,
        },
        _ => err,
    }
}

pub fn type_mismatch(_loc_func: Location, want: &Type, loc_arg: Location, give: &Type) -> Error {
    // TODO: pass enough context to have AsNthArgToNowTyped/LetAlreadyApplied
    Error(
        loc_arg,
        ExpectedButGot {
            expected: want.deep_clone(),
            actual: give.deep_clone(),
        },
    )
}

pub fn already_declared(
    name: String,
    loc_old: Location,
    type_old: &Type,
    loc_new: Location,
) -> Error {
    Error(
        loc_old,
        ContextCaused {
            error: Box::new(Error(loc_new, NameAlreadyDeclared { name })),
            because: DeclaredHereWithType {
                with_type: type_old.deep_clone(),
            },
        },
    )
}

pub fn context_fallback_required(loc_pat: Location, err: Error) -> Error {
    Error(
        loc_pat,
        ContextCaused {
            error: err.into(),
            because: LetFallbackRequired,
        },
    )
}

pub fn context_fallback_mismatch(loc_idk: Location, err: Error, res: &Type, alt: &Type) -> Error {
    Error(
        loc_idk,
        ContextCaused {
            error: err.into(),
            because: LetFallbackTypeMismatch {
                result_type: res.deep_clone(),
                fallback_type: alt.deep_clone(),
            },
        },
    )
}

pub fn context_extra_comma_makes_rest(loc_comma: Location, err: Error) -> Error {
    Error(
        loc_comma,
        ContextCaused {
            error: err.into(),
            because: ListExtraCommaMakesRest,
        },
    )
}

/*
pub fn context_complete_type(
    types: &TypeList,
    loc: Location,
    err: ErrorKind,
    ty: TypeRef,
) -> ErrorKind {
    let complete_type = types.frozen(ty);
    match &err {
        ExpectedButGot {
            expected: _,
            actual,
        } if complete_type != *actual => ContextCaused {
            error: Box::new(Error(loc, err)),
            because: CompleteType { complete_type },
        },
        _ => err,
    }
}

pub fn context_auto_coerced(
    arg_loc: Location,
    err: ErrorKind,
    func_name: String,
    func_type: Rc<Type>,
) -> ErrorKind {
    ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: AutoCoercedVia {
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
    ContextCaused {
        error: Box::new(Error(arg_loc, err)),
        because: {
            // unreachable: func is a function, see `err_context_as_nth_arg` call sites
            let (nth_arg, func) = match &func.value {
                TreeKind::Apply(app, args) => (args.len() + 1, app.clone()),
                _ => unreachable!(),
            };
            match comma_loc {
                Some(comma_loc) => ChainedFromAsNthArgToNowTyped {
                    comma_loc,
                    nth_arg,
                    func,
                    type_with_curr_args,
                },
                None => AsNthArgToNowTyped {
                    nth_arg,
                    func,
                    type_with_curr_args,
                },
            }
        },
    }
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
        ContextCaused {
            error: Box::new(Error(
                item_loc.clone(),
                context_complete_type(types, item_loc, err, item_type),
            )),
            because: TypeListInferredItemType {
                list_item_type: types.frozen(list_item_type),
            },
        },
    )
}

*/
// }}}

// generate report {{{
fn nth(n: usize) -> String {
    format!(
        "{n}{}",
        match n {
            1 => "st",
            2 => "nd",
            3 => "rd",
            _ => "th",
        }
    )
}
impl Error {
    fn ctx_messages(loc: Location, because: &ErrorContext, report: &mut Report) {
        let msgs: &[_] = match because {
            AsNthArgToNowTyped {
                nth_arg,
                func,
                type_with_curr_args,
            } => &[(
                loc,
                format!(
                    "see parameter in {type_with_curr_args} (overall {} argument to {:?})",
                    nth(*nth_arg),
                    func
                ),
            )],
            AutoCoercedVia {
                func_name,
                func_type,
            } => &[(
                loc,
                format!("coerced via '{func_name}' (which has type {func_type})"),
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
                        "see parameter in {type_with_curr_args} (overall {} argument to {:?})",
                        nth(*nth_arg),
                        func
                    ),
                ),
                (comma_loc.clone(), "chained through here".into()),
            ],
            ChainedFromToNotFunc { comma_loc } => &[
                (loc, "Not a function".into()),
                (comma_loc.clone(), "chained through here".into()),
            ],
            CompleteType { complete_type } => &[(loc, format!("complete type: {complete_type}"))],
            DeclaredHereWithType { with_type } => {
                &[(loc, format!("declared here with type {with_type}"))]
            }
            FuncTooManyArgs { nth_arg, func } => &[(
                loc,
                format!(
                    "Too many argument to {:?}, expected only {}",
                    func,
                    nth_arg - 1
                ),
            )],
            LetAlreadyApplied => &[(loc, format!("this binding is already applied an argument"))],
            LetFallbackRequired => &[(
                loc,
                "pattern is refutable so a fallback is required".to_string(),
            )],
            LetFallbackTypeMismatch {
                result_type,
                fallback_type,
            } => &[(
                loc,
                format!("fallback of type {fallback_type} doesn't match result type {result_type}"),
            )],
            ListExtraCommaMakesRest => {
                &[(loc, format!("this extra comma makes the last item a list"))]
            }
            ListTypeInferredItemType { list_item_type } => &[(
                loc,
                format!("list type was inferred to be [{list_item_type}]"),
            )],
            Unmatched { open_token } => &[(loc, format!("{open_token} here"))],
        };
        report.messages.extend_from_slice(msgs);
    }

    pub fn report<'a>(&self, registry: &'a ModuleRegistry) -> Report<'a> {
        let loc = self.0.clone();
        match &self.1 {
            ContextCaused { error, because } => {
                let mut r = error.report(registry);
                Error::ctx_messages(loc, because, &mut r);
                r
            }
            CouldNotReadFile { error } => Report {
                registry,
                title: "Could not read file".into(),
                messages: vec![(loc, format!("{error}"))],
            },
            ExpectedButGot { expected, actual } => Report {
                registry,
                title: "Type mismatch".into(),
                // TODO: (could) if expression of type `actual` is a name, search in scope,
                //               otherwise search for function like `actual -> expected`
                messages: vec![(loc, format!("Expected type {expected}, but got {actual}"))],
            },
            InconsistentType { types } => Report {
                registry,
                title: "Inconsistent type from usage".into(),
                messages: types
                    .iter()
                    .map(|(loc, ty)| (loc.clone(), format!("Use here with type {ty}")))
                    .collect(),
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
            NotFunc { actual_type } => Report {
                registry,
                title: "Not a function".into(),
                messages: vec![(
                    loc,
                    format!("Expected a function type, but got {actual_type}"),
                )],
            },
            Unexpected { token, expected } => Report {
                registry,
                title: "Unexpected token".into(),
                messages: vec![(loc, format!("Unexpected {token}, expected {expected}"))],
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
                    if matches!(**expected_type, Type::Named(_, _)) {
                        format!("Unknown name '{name}' (may be of any type)")
                    } else {
                        format!("Unknown name '{name}', should be of type {expected_type}")
                    },
                )],
            },
            Utf8Error { text: _, error } => Report {
                registry,
                title: "Utf8 error while decoding".into(),
                messages: vec![(loc, format!("{error}"))],
            },
        }
    }
}

pub fn report_many_stderr<'a>(
    errors: impl IntoIterator<Item = &'a Error>,
    registry: &ModuleRegistry,
    was_file: &Option<String>,
    used_file: bool,
) {
    let mut count = 0;
    let use_colors = std::io::stderr().is_terminal();
    if use_colors {
        for e in errors {
            count += 1;
            eprintln!("{:#}", e.report(&registry));
        }
    } else {
        for e in errors {
            count += 1;
            eprintln!("{}", e.report(&registry));
        }
    }
    eprintln!("({} error{})", count, if 1 == count { "" } else { "s" });

    if let Some(name) = was_file {
        if used_file {
            eprintln!("Note: {name} was interpreted as a file name");
        } else {
            eprintln!("Note: {name} is also a file, but it did not start with '#!'");
        }
    }
}
// }}}

// display report {{{
impl Display for Report<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        let use_colors = f.alternate();
        let ctop: &str = if use_colors { "\x1b[33m" } else { "" };
        let cmsg: &str = if use_colors { "\x1b[34m" } else { "" };
        let carr: &str = if use_colors { "\x1b[35m" } else { "" };
        let cnum: &str = if use_colors { "\x1b[36m" } else { "" };
        let cerr: &str = if use_colors { "\x1b[31m" } else { "" };
        let r: &str = if use_colors { "\x1b[m" } else { "" };

        let (Location(file, range), _) = &self.messages[0];
        let source = match self.registry.load(file) {
            Ok(module) => module,
            Err(e) => return writeln!(f, "{cerr}cannot get source {file:?} anymore{r}:\n{e:?}"),
        };

        let lnum = source.get_containing_lnum(range.start).unwrap();
        writeln!(f, "{}:{lnum}: {ctop}{}{r}", source.path, self.title)?;

        for (Location(file, range), msg) in &self.messages {
            let source = match self.registry.load(file) {
                Ok(module) => module,
                Err(e) => {
                    writeln!(f, "{cerr}cannot get source {file:?} anymore{r}:\n{e:?}")?;
                    continue;
                }
            };

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
