/// XXX: everything is pain, but can't be bothered; this will do for now (and maybe more)

extern crate proc_macro;
use proc_macro::{TokenStream, TokenTree, Ident, Span, Punct, Spacing, Group, Delimiter};

type Toks = Vec<TokenTree>;

/// `quote!`, but more less better at everything
macro_rules! tts {
    ($h:expr, $($t:expr),+) => { tts!($h)$(.chain(tts!($t)))+ };
    ($h:expr, $($t:expr),+,) => { tts!($h)$(.chain(tts!($t)))+ };
    ($text:literal) => { $text.parse::<TokenStream>().unwrap().into_iter() };
    ($expr:expr) => { $expr.into_iter() };
    ($text:literal,) => { tts!($text) };
    ($expr:expr,) => { tts!($expr) };
    () => { tts!(vec![]) }
}

fn parse(rs: &str) -> Toks {
    rs.parse::<TokenStream>().unwrap().into_iter().collect()
}
fn ident(name: &str) -> Toks {
    vec![TokenTree::Ident(Ident::new(name, Span::call_site()))]
}
// fn lit(v: Literal) -> Toks {
//     vec![TokenTree::Literal(v)]
// }
fn punct(s: &str) -> Toks {
    let len = s.len();
    let mut iter = s.chars();
    let mut crap: Toks = iter
        .by_ref()
        .take(len-1)
        .map(|c| TokenTree::Punct(Punct::new(c, Spacing::Joint)))
        .collect();
    crap.push(TokenTree::Punct(Punct::new(iter.next().unwrap(), Spacing::Alone)));
    crap
}
fn group<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
    vec![TokenTree::Group(Group::new(Delimiter::None, content.collect()))]
}
fn parents<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
    vec![TokenTree::Group(Group::new(Delimiter::Parenthesis, content.collect()))]
}
fn braces<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
    vec![TokenTree::Group(Group::new(Delimiter::Brace, content.collect()))]
}
// fn bracks<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
//     vec![TokenTree::Group(Group::new(Delimiter::Bracket, content.collect()))]
// }

struct Param {
    // source: Toks,
    as_value: String, // TODO: this a hack, construct and destruct properly (eg. `Array`s)
}

fn from_dsl_ty(tts: Toks) -> Vec<Param> {
    if 0 == tts.len() {
        vec![]
    } else {
        let mut iter = tts.into_iter();

        let head: Toks = iter
            .by_ref()
            .take_while(|t| {
                if let TokenTree::Punct(p) = t {
                    if Spacing::Alone == p.spacing() { panic!("expected token ->"); }
                    false
                } else { true }
            })
            .collect();

        match iter.next() {
            Some(TokenTree::Punct(p))
                if ':' == p.as_char()
                && Spacing::Alone == p.spacing() => panic!("expected token ->"),
            _ => (),
        }

        let tail: Toks = iter.collect();

        // TODO: proper, this quick hack
        let as_value = match &head[0] {
            TokenTree::Ident(i) => {
                if i.to_string().chars().next().unwrap().is_ascii_uppercase() { format!("Value::{i}") }
                else { "Value::Garbo".to_string() }
            },
            TokenTree::Group(p) if Delimiter::Bracket == p.delimiter() => {
                "Value::Arr".to_string()
            },
            other => panic!("incorrect type representation near {other}"),
        };

        vec![Param {
            // source: head,
            as_value,
        }]
            .into_iter()
            .chain(from_dsl_ty(tail))
            .collect()
    }
}

fn wrap_fn_this(bla: Toks) -> Toks {
    group(tts!(
        punct("|"), ident("this"), punct("|"),
        bla
    ))
}

// wrap fn_tail around the match that extracts args (as well as the call)
fn wrap_extract_call<T>(params: Vec<Param>, ret_last: Param, fn_tail: T) -> Toks where T: Iterator<Item=TokenTree> {
    let nexts = params
        .iter()
        .map(|_| parse("args_iter.next(),"))
        .flatten();
    let arg_n = params
        .iter()
        .enumerate()
        .map(|(k, _)| parse(&format!("args{k},")))
        .flatten();

    let match_pat = parents(params
        .iter()
        .enumerate()
        .map(|(k, p)| parse(&format!("Some({}(args{k})),", p.as_value))) // TODO
        .flatten());

    // TODO: wrap result in a Value::[..]
    tts!(
        parse(&ret_last.as_value), parents(tts!(
            braces(tts!(
                parse("let mut args_iter = this.args.into_iter();"),
                ident("match"), parents(nexts), braces(tts!(
                    match_pat, punct("=>"), parents(fn_tail), parents(arg_n),
                    parse(", _ => unreachable!()"),
                ))
            ))
        ))
    ).collect()
}

fn function<T>(name_ident: Ident, params: Vec<Param>, ret_last: Param, fn_tail: T, iter_n: usize) -> Toks where T: Iterator<Item=TokenTree>, {
    let (name, args) = if 0 == iter_n {
        ( tts!(parse(&format!("\"{name_ident}\".to_string()")))
        , tts!(parse("vec![]"))
        )
    } else {
        ( tts!(parse("this.name"))
        , tts!(parse("this.args"))
        )
    };

    let (_maps, func) = if params.len()-1 == iter_n {
        ( ()
        , wrap_fn_this(wrap_extract_call(params, ret_last, fn_tail))
        )
    } else {
        ( ()
        , wrap_fn_this(function(name_ident, params, ret_last, fn_tail, iter_n+1))
        )
    };

    tts!(
        parse("Value::Fun"), parents(tts!(
            ident("Function"), braces(tts!(
                parse("  name:"), name,
                parse(", maps:"), parents(tts!()),
                parse(", args:"), args,
                parse(", func:"), func,
            ))
        ))
    ).collect()
}

fn value<T>(_name_ident: Ident, ret_last: Param, val_tail: T) -> Toks where T: Iterator<Item=TokenTree>, {
    // TODO: wrap result in a Value::[..]
    // (this is the same as in `wrap_extract_call` and, to some extent, in `function`)
    tts!(
        parse(&ret_last.as_value),
        parents(val_tail)
    ).collect()
}

#[proc_macro]
pub fn val(stream: TokenStream) -> TokenStream {
    let mut it = stream.into_iter();

    let name = match it.next() {
        Some(TokenTree::Ident(a)) => a,
        _ => panic!("expected identifier (name)"),
    };

    match (it.next(), it.next()) {
        (Some(TokenTree::Punct(a)), Some(TokenTree::Punct(b)))
            if ':' != a.as_char()
            || ':' != b.as_char()
            || a.spacing() == b.spacing()
            => panic!("expected token ::"),
        _ => (),
    }

    let ty: Toks = it
        .by_ref()
        .take_while(|t| "=" != t.to_string())
        .collect();

    let val: Toks = it.collect();
    if 0 == val.len() { panic!("expected = followed by an expression") }

    let mut types = from_dsl_ty(ty);
    let ret = types.pop().unwrap();

    if 0 == types.len() {
        tts!(value(name, ret, tts!(val))).collect()
    } else {
        tts!(function(name, types, ret, tts!(val), 0)).collect()
    }
}
