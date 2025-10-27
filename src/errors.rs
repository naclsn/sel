//! error structs, helpers and reporting

use std::fmt::{Display, Formatter, Result as FmtResult};
use std::io::{Error as IoError, IsTerminal};
use std::rc::Rc;

use crate::check::{Refers, Tree, TreeVal};
use crate::lex::{Token, TokenKind};
use crate::module::{Location, ModuleRegistry};
use crate::types::Type;

// error types {{{
#[derive(Debug)]
pub enum MismatchAs {
    ItemOf,
    RetOf,
    ParOf,
    FstOf,
    SndOf,
    Wanted(Rc<Type>), // `deep_clone`'d
}

impl Display for MismatchAs {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            MismatchAs::ItemOf => write!(f, "item of"),
            MismatchAs::RetOf => write!(f, "return of"),
            MismatchAs::ParOf => write!(f, "parameter of"),
            MismatchAs::FstOf => write!(f, "first of"),
            MismatchAs::SndOf => write!(f, "second of"),
            MismatchAs::Wanted(ty) => write!(f, "{ty}"),
        }
    }
}

#[derive(Debug)]
pub enum ErrorContext {
    AsNthArgToNowTyped {
        nth_arg: usize,
        type_with_curr_args: Rc<Type>,
    },
    /*
    AutoCoercedVia {
        func_name: String,
        func_type: Rc<Type>,
    },
    */
    ChainedFromAsNthArgToNowTyped {
        comma_loc: Location,
        nth_arg: usize,
        type_with_curr_args: Rc<Type>,
    },
    ChainedFromToNotFunc {
        comma_loc: Location,
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
    UseCannotLoad {
        #[allow(unused)]
        prefix: String,
        error: IoError,
    },
    UseNotAtTopForPrefix {
        prefix: String,
    },
    UseModuleDoesNotHave {
        prefix: String,
        name: String,
    },
}

#[derive(Debug)]
pub enum ErrorKind {
    ContextCaused {
        error: Box<Error>,
        because: ErrorContext,
    },
    ExpectedButGot {
        expected: Rc<Type>,
        actual: Rc<Type>,
        as_of: Vec<MismatchAs>, // expected to always be a sequence of [frag]Of with a final Wanted
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
        available: Box<[String]>,
    },
    Utf8Error {
        #[allow(unused)]
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

pub fn unknown_name<'a>(
    loc: Location,
    name: String,
    ty: Rc<Type>,
    avail: impl IntoIterator<Item = &'a str>,
) -> Error {
    Error(
        loc,
        UnknownName {
            name,
            expected_type: ty,
            // TODO: filter for similar names (and for matching types..? maybe don have the ty yet)
            available: [].into(), //avail.into_iter().map(|s| s.into()).collect(),
        },
    )
}

pub fn not_function(
    loc_sad: Location,
    loc_basemost: Location,
    ty: &Type,
    maybe_func_with_too_many_args: &Tree,
) -> Error {
    let err = Error(
        loc_sad,
        NotFunc {
            actual_type: ty.deep_clone(),
        },
    );

    match &maybe_func_with_too_many_args.val {
        TreeVal::Word(_, prov) => context_declared_here(ty, prov, err),

        TreeVal::Apply(base, args) => match &base.val {
            TreeVal::Word(name, prov) => context_declared_here(
                ty,
                prov,
                Error(
                    loc_basemost,
                    ContextCaused {
                        error: err.into(),
                        because: FuncTooManyArgs {
                            nth_arg: args.len() + 1,
                            func: name.clone(),
                        },
                    },
                ),
            ),

            TreeVal::Binding(_, _, _) => Error(
                loc_basemost,
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

pub fn type_mismatch(
    loc_basemost: Location,
    _loc_func: Location,
    loc_arg: Location,
    want: &Type,
    give: &Type,
    as_of: Vec<MismatchAs>,
    maybe_func_with_other_param: Option<&Tree>,
) -> Error {
    let err = Error(
        loc_arg,
        ExpectedButGot {
            expected: want.deep_clone(),
            actual: give.deep_clone(),
            as_of,
        },
    );

    if let Some(maybe_func_with_other_param) = maybe_func_with_other_param {
        match &maybe_func_with_other_param.val {
            TreeVal::Word(_, _) => Error(
                loc_basemost,
                ContextCaused {
                    error: err.into(),
                    because: AsNthArgToNowTyped {
                        nth_arg: 1,
                        type_with_curr_args: maybe_func_with_other_param.ty.deep_clone(),
                    },
                },
            ),

            TreeVal::Apply(base, args) => Error(
                loc_basemost,
                ContextCaused {
                    error: err.into(),
                    because: AsNthArgToNowTyped {
                        nth_arg: args.len() + 1,
                        type_with_curr_args: base.ty.deep_clone(),
                    },
                },
            ),

            _ => err,
        }
    } else {
        err
    }
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

pub fn context_declared_here(ty: &Type, prov: &Refers, err: Error) -> Error {
    let loc_decl = match prov {
        Refers::Binding(loc) => loc.clone(),
        Refers::Defined(func) => func.loc.clone(),
        Refers::File(_module, func) => func.loc.clone(),
        _ => return err,
    };
    Error(
        loc_decl,
        ContextCaused {
            error: err.into(),
            because: DeclaredHereWithType {
                with_type: ty.deep_clone(),
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

pub fn context_cannot_load(
    prefix: String,
    loc_use: Location,
    io_err: IoError,
    err: Error,
) -> Error {
    Error(
        loc_use,
        ContextCaused {
            error: err.into(),
            because: UseCannotLoad {
                prefix,
                error: io_err,
            },
        },
    )
}

pub fn context_no_use_for_prefix(prefix: String, err: Error) -> Error {
    Error(
        Location(err.0 .0.clone(), 0..1),
        ContextCaused {
            error: err.into(),
            because: UseNotAtTopForPrefix { prefix },
        },
    )
}

pub fn context_not_in_module(prefix: String, loc_use: Location, name: String, err: Error) -> Error {
    Error(
        loc_use,
        ContextCaused {
            error: err.into(),
            because: UseModuleDoesNotHave { prefix, name },
        },
    )
}

/*
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
                type_with_curr_args,
            } => &[(
                loc,
                format!(
                    "see parameter in {type_with_curr_args} (overall {} argument)",
                    nth(*nth_arg),
                ),
            )],
            /*
            AutoCoercedVia {
                func_name,
                func_type,
            } => &[(
                loc,
                format!("coerced via '{func_name}' (which has type {func_type})"),
            )],
            */
            ChainedFromAsNthArgToNowTyped {
                comma_loc,
                nth_arg,
                type_with_curr_args,
            } => &[
                (
                    loc,
                    format!(
                        "see parameter in {type_with_curr_args} (overall {} argument to)",
                        nth(*nth_arg),
                    ),
                ),
                (comma_loc.clone(), "chained through here".into()),
            ],
            ChainedFromToNotFunc { comma_loc } => &[
                (loc, "Not a function".into()),
                (comma_loc.clone(), "chained through here".into()),
            ],
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
            LetAlreadyApplied => &[(loc, "this binding is already applied an argument".into())],
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
                &[(loc, "this extra comma makes the last item a list".into())]
            }
            ListTypeInferredItemType { list_item_type } => &[(
                loc,
                format!("list type was inferred to be [{list_item_type}]"),
            )],
            Unmatched { open_token } => &[(loc, format!("{open_token} here"))],
            UseCannotLoad { prefix: _, error } => &[(loc, format!("cannot load module: {error}"))],
            UseNotAtTopForPrefix { prefix } => &[(
                loc,
                format!("no 'use' declaring the prefix '{prefix}' in this module"),
            )],
            UseModuleDoesNotHave { prefix, name } => &[(
                loc,
                format!("no such name in this module: '{name}' ('{prefix}' was the prefix)"),
            )],
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
            ExpectedButGot {
                expected,
                actual,
                as_of,
            } => Report {
                registry,
                title: "Type mismatch".into(),
                // TODO: (could) if expression of type `actual` is a name, search in scope,
                //               otherwise search for function like `actual -> expected`
                messages: vec![(
                    loc,
                    format!(
                        "Expected type {expected}, but got {actual}{}",
                        if as_of.len() < 2 {
                            "".to_string()
                        } else {
                            format!(
                                " ({})",
                                as_of
                                    .iter()
                                    .map(ToString::to_string)
                                    .collect::<Vec<_>>()
                                    .join(" "),
                            )
                        },
                    ),
                )],
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
                available,
            } => Report {
                registry,
                title: "Unknown name".into(),
                messages: vec![(
                    loc,
                    if matches!(&**expected_type, Type::Named(_, tr) if tr.borrow().is_none()) {
                        format!("Unknown name '{name}' (may be of any type) {available:?}")
                    } else {
                        format!("Unknown name '{name}', should be of type {expected_type} {available:?}")
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
            eprintln!("{:#}", e.report(registry));
        }
    } else {
        for e in errors {
            count += 1;
            eprintln!("{}", e.report(registry));
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
