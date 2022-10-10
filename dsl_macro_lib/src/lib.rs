extern crate proc_macro;
use proc_macro::{Delimiter, Group, Ident, Spacing, TokenStream, TokenTree};

type Toks = Vec<TokenTree>;

/// `quote!`, but more less better at everything
macro_rules! tts {
    ($h:expr, $($t:expr),+) => { tts!($h)$(.chain(tts!($t)))+ };
    ($text:literal) => { $text.parse::<TokenStream>().unwrap().into_iter() };
    ($expr:expr) => { $expr.into_iter() };
}

fn parse(rs: &str) -> Toks {
    rs.parse::<TokenStream>().unwrap().into_iter().collect()
}
fn group<T>(content: T) -> Toks
where
    T: Iterator<Item = TokenTree>,
{
    vec![TokenTree::Group(Group::new(
        Delimiter::None,
        content.collect(),
    ))]
}
fn parents<T>(content: T) -> Toks
where
    T: Iterator<Item = TokenTree>,
{
    vec![TokenTree::Group(Group::new(
        Delimiter::Parenthesis,
        content.collect(),
    ))]
}
fn braces<T>(content: T) -> Toks
where
    T: Iterator<Item = TokenTree>,
{
    vec![TokenTree::Group(Group::new(
        Delimiter::Brace,
        content.collect(),
    ))]
}
// fn bracks<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
//     vec![TokenTree::Group(Group::new(Delimiter::Bracket, content.collect()))]
// }

#[derive(Debug, Clone)]
enum Type {
    Num,
    Str,
    Lst(Box<Type>),
    Fun(Box<Type>, Box<Type>),
    Unk(String),
    Now(String),
}

impl Type {
    /// Here, atom can be:
    ///  - Type
    ///  - name
    ///  - [<arr>]
    ///  - (<fun>)
    /// Uses the iterator as far as needed (so parsing <fun> can be recursive).
    fn new_atom<T>(mut source: T) -> Self
    where
        T: Iterator<Item = TokenTree>,
    {
        match source.next() {
            Some(TokenTree::Ident(i)) => match i.to_string().as_str() {
                "Num" => Type::Num,
                "Str" => Type::Str,
                name if name.chars().next().unwrap().is_ascii_lowercase() => {
                    Type::Unk(name.to_string())
                }
                other => panic!("expected type identifier, got '{other}'"),
            },

            Some(TokenTree::Group(p)) if Delimiter::Bracket == p.delimiter() => {
                Type::Lst(Box::new(Type::new_atom(p.stream().into_iter())))
            }

            Some(TokenTree::Group(p)) if Delimiter::Parenthesis == p.delimiter() => {
                to_fun_ty(from_dsl_ty(p.stream().into_iter().collect())) // note that it may not be a Type::Fun in the end...
            }

            Some(other) => panic!("expected type atom, got '{other}'"),
            None => panic!("expected type atom"),
        }
    }

    // List the names of not yet known type variables
    // (ie. every `Unk`).
    fn unknowns(&self) -> Vec<String> {
        match self {
            Type::Num | Type::Str | Type::Now(_) => vec![],
            Type::Lst(a) => a.unknowns(),
            Type::Fun(a, b) => [a.unknowns(), b.unknowns()].concat(),
            Type::Unk(name) => vec![name.clone()],
        }
    }

    // Replace occurences of `Unk(name)` with `Now(name)`
    // for every names in `known`.
    fn now_known(self, known: &Vec<String>) -> Type {
        match self {
            Type::Num | Type::Str | Type::Now(_) => self.clone(),
            Type::Lst(a) => Type::Lst(Box::new(a.now_known(known))),
            Type::Fun(a, b) => {
                Type::Fun(Box::new(a.now_known(known)), Box::new(b.now_known(known)))
            }
            Type::Unk(name) => {
                if known.contains(&name) {
                    Type::Now(name)
                } else {
                    Type::Unk(name)
                }
            }
        }
    }

    /// (will) Create the appropriate Type value, eg.:
    ///     Type::Lst(Type::Num)
    ///     Type::Unk("a".to_string())
    ///     a // when it's a now known type (then is has been update via now_known)
    fn construct(&self) -> Toks {
        match self {
            Type::Num => parse("Type::Num"),
            Type::Str => parse("Type::Str"),

            Type::Lst(a) => tts!(
                parse("Type::Lst"),
                parents(tts!(parse("Box::new"), parents(tts!(a.construct()))))
            )
            .collect(),

            Type::Fun(a, b) => tts!(
                parse("Type::Fun"),
                parents(tts!(
                    parse("  Box::new"),
                    parents(tts!(a.construct())),
                    parse(", Box::new"),
                    parents(tts!(b.construct()))
                ))
            )
            .collect(),

            Type::Unk(name) => tts!(
                parse("Type::Unk"),
                parents(tts!(parse(&format!("\"{name}\".to_string()"))))
            )
            .collect(),

            // needs cloning because a same type may appear
            // multiple times in a function's `maps`
            Type::Now(name) => parse(&format!("{name}.clone()")),
        }
    }

    /// (will) Destructure the given expression,
    /// extracting the unkown into same-name variables.
    fn destruct(&self, expr: Toks) -> Toks {
        match self {
            Type::Num => vec![], //tts!(parse("let /*sure*/ Type::Num = "), expr, parse(";")).collect(),
            Type::Str => vec![], //tts!(parse("let /*sure*/ Type::Str = "), expr, parse(";")).collect(),

            Type::Lst(a) => tts!(
                parse("let (type_c) = match "),
                expr,
                braces(tts!(parse(
                    "Type::Lst(box_c) => (*box_c), _ => unreachable!()"
                ))),
                parse(";"),
                a.destruct(parse("type_c"))
            )
            .collect(),

            Type::Fun(a, b) => tts!(
                parse("let (type_a, type_b) = match "),
                expr,
                braces(tts!(parse(
                    "Type::Fun(box_a, box_b) => (*box_a, *box_b), _ => unreachable!()"
                ))),
                parse(";"),
                a.destruct(parse("type_a")),
                b.destruct(parse("type_b"))
            )
            .collect(),

            Type::Unk(name) => tts!(parse(&format!("let {name} = ")), expr, parse(";")).collect(),

            Type::Now(_name) => vec![], //parse(&format!("let /*sure*/ {name} = {name};")),
        }
    }
}

/// Parse the atoms in a function type declaration
/// (ie. separated by '->').
fn from_dsl_ty(tts: Toks) -> Vec<Type> {
    if 0 == tts.len() {
        vec![]
    } else {
        let mut iter = tts.into_iter();
        let mut found_arrop = false;

        let head: Toks = iter
            .by_ref()
            .take_while(|t| {
                if let TokenTree::Punct(p) = t {
                    if '-' != p.as_char() || Spacing::Joint != p.spacing() {
                        panic!("expected token '->', got {}", TokenTree::Punct(p.clone()));
                    }
                    found_arrop = true;
                    false
                } else {
                    true
                }
            })
            .collect();
        if 0 == head.len() {
            match iter.next() {
                Some(other) => panic!("expected type atom, got '{other}'"),
                None => panic!("expected type atom"),
            }
        }

        if found_arrop {
            match iter.next() {
                Some(TokenTree::Punct(p))
                    if '>' != p.as_char() || Spacing::Alone != p.spacing() =>
                {
                    panic!("expected token '->', got {}", TokenTree::Punct(p))
                }
                _ => (),
            }
        }
        let tail: Toks = iter.collect();
        if !found_arrop && 0 != tail.len() {
            panic!("expected token '=', got {}", tail[0]);
        }

        [Type::new_atom(head.into_iter())]
            .into_iter()
            .chain(from_dsl_ty(tail))
            .collect()
    }
}

/// Join back the atoms parsed by `from_dsl_ty`
/// into a(n actual) Type::Fun.
/// Note that as a consequence, if the vec contains
/// a single Type then it will be this one.
fn to_fun_ty(crap: Vec<Type>) -> Type {
    crap.into_iter()
        .rev()
        .reduce(|acc, cur| Type::Fun(Box::new(cur), Box::new(acc)))
        .unwrap()
}

/// (will) Call fn_tail with and after extracting the
/// (correctly typed) args.
fn wrap_extract_call<T>(params_and_ret: Vec<Type>, fn_tail: T) -> Toks
where
    T: Iterator<Item = TokenTree>,
{
    let mut params = params_and_ret;
    params.pop();

    let fn_args = params
        .iter()
        .map(|_| parse("args_iter.next().unwrap().into(),"))
        .flatten();

    tts!(
        parse("Value::from"),
        parents(tts!(braces(tts!(
            parse("let mut args_iter = this.args.into_iter();"),
            parents(fn_tail),
            parents(fn_args)
        ))))
    )
    .collect()
}

fn function<T>(name_ident: Ident, params_and_ret: Vec<Type>, fn_tail: T, iter_n: usize) -> Toks
where
    T: Iterator<Item = TokenTree>,
{
    let (name, args) = if 0 == iter_n {
        (
            tts!(parse(&format!("\"{name_ident}\".to_string()"))),
            tts!(parse("vec![]")),
        )
    } else {
        (tts!(parse("this.name")), tts!(parse("this.args")))
    };

    let mut iter = params_and_ret.into_iter();
    let settled: Vec<Type> = iter.by_ref().take(iter_n).collect(); // this first part was used in previous iteration, not here
    let head = iter.next().unwrap(); // head is the type of the latest (will) applied argument
    let tail: Vec<Type> = iter.collect(); // tail it the return type, to be updated with info from the latest arg's type

    assert!(
        0 < tail.len(),
        "[internal] one too many iteration in `function` (or missed call to `value`)"
    );

    let type_a = head.construct();
    let type_b = to_fun_ty(tail.clone()).construct();
    let maps = tts!(parents(tts!(type_a, parse(","), type_b)));

    let names = head.unknowns();
    let niw_head = head.clone();
    let niw_tail: Vec<Type> = tail.into_iter().map(|it| it.now_known(&names)).collect();
    let niw_params_and_ret = [settled, vec![niw_head], niw_tail].concat();

    let type_extract: Toks = niw_params_and_ret[0..iter_n + 1]
        .iter()
        .enumerate()
        .map(|(k, it)| it.destruct(parse(&format!("this.args.index({k}).typed()"))))
        .flatten()
        .collect();

    let func = group(tts!(
        parse("|this|"),
        braces(tts!(
            type_extract,
            if niw_params_and_ret.len() - 2 == iter_n {
                wrap_extract_call(niw_params_and_ret, fn_tail)
            } else {
                function(name_ident, niw_params_and_ret, fn_tail, iter_n + 1)
            }
        ))
    ));

    tts!(
        parse("Value::Fun"),
        parents(tts!(
            parse("Function"),
            braces(tts!(
                parse("  name:"),
                name,
                parse(", maps:"),
                maps,
                parse(", args:"),
                args,
                parse(", func:"),
                func
            ))
        ))
    )
    .collect()
}

fn value<T>(_name_ident: Ident, _ret_last: Type, val_tail: T) -> Toks
where
    T: Iterator<Item = TokenTree>,
{
    tts!(parse("Value::from"), parents(val_tail)).collect()
}

#[proc_macro]
pub fn val(stream: TokenStream) -> TokenStream {
    let mut it = stream.into_iter();

    let name = match it.next() {
        Some(TokenTree::Ident(name)) => name,
        Some(other) => panic!("expected identifier (name), got '{other}'"),
        None => panic!("expected identifier (name)"),
    };

    match (it.next(), it.next()) {
        (Some(TokenTree::Punct(a)), Some(TokenTree::Punct(b)))
            if ':' != a.as_char() || ':' != b.as_char() || a.spacing() == b.spacing() =>
        {
            match a.spacing() {
                Spacing::Joint => panic!(
                    "expected token '::', got '{}{}'",
                    TokenTree::Punct(a),
                    TokenTree::Punct(b)
                ),
                Spacing::Alone => panic!("expected token '::', got '{}'", TokenTree::Punct(a)),
            }
        }
        _ => (),
    }

    let ty: Toks = it.by_ref().take_while(|t| "=" != t.to_string()).collect();
    if 0 == ty.len() {
        match it.next() {
            Some(other) => panic!("expected type expression (type), got '{other}'"),
            None => panic!("expected type expression (type)"),
        }
    }

    let val: Toks = it.collect();
    if 0 == val.len() {
        panic!("expected token '=' then expression (value)");
    }

    let types = from_dsl_ty(ty);

    if 1 == types.len() {
        tts!(value(name, types.into_iter().next().unwrap(), tts!(val))).collect()
    } else {
        tts!(function(name, types, tts!(val), 0)).collect()
    }
}
