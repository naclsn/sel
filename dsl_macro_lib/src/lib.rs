extern crate proc_macro;
use proc_macro::{TokenStream, TokenTree, Ident, Spacing, Group, Delimiter};

type Toks = Vec<TokenTree>;

/// `quote!`, but more less better at everything
macro_rules! tts {
    ($h:expr, $($t:expr),+) => { tts!($h)$(.chain(tts!($t)))+ };
    ($text:literal) => { $text.parse::<TokenStream>().unwrap().into_iter() };
    ($expr:expr) => { $expr.into_iter() };

    ($h:expr, $($t:expr),+,) => { tts!($h, $($t),+) };
    ($text:literal,) => { tts!($text) };
    ($expr:expr,) => { tts!($expr) };

    () => { tts!([]) };
}

fn parse(rs: &str) -> Toks {
    rs.parse::<TokenStream>().unwrap().into_iter().collect()
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

// YYY: cannot use the one from main crate (circular deps, I guess..?)
// TODO: from_token_stream or something
#[derive(Debug, Clone)]
enum Type {
    Num,
    Str,
    Arr(Box<Type>),
    Fun(Box<Type>, Box<Type>),
    Unk(String),
}

// XXX: can (will) probly drop the wrapping and use the enum directly
#[derive(Clone)]
struct Param {
    _source: Toks,
    plain_name: String,
    ty: Type,
}

impl Param {
    fn new(source: Toks) -> Self {
        // TODO: proper, this quick hack (eg. parenthesized types)
        let plain_name = match &source[0] {
            TokenTree::Ident(i) => {
                if i.to_string().chars().next().unwrap().is_ascii_uppercase() { format!("Value::{i}") }
                else { "Value::Garbo".to_string() }
            },
            TokenTree::Group(p) if Delimiter::Bracket == p.delimiter() => {
                "Value::Arr".to_string()
            },
            other => panic!("incorrect type representation near {other}"),
        };

        Self {
            _source: source,
            plain_name,
            ty: (),
        }
    }

    fn _repr(&self) -> String {
        self._source
            .iter()
            .map(|t| t.to_string())
            .collect::<Vec<String>>()
            .join("+")
    }

    // TODO
    /// (will) Create the appropriate Type value, eg.:
    ///     Type::Arr(Type::Num)
    ///     Type::Unk("a".to_string())
    ///     a // when it's a now known type (TODO: info will have to come from somewhere - add needed param)
    fn as_type<T>(&self) -> Toks where T: Iterator<Item=TokenTree> {
        parse("Type::Num")
    }

    // TODO
    /// (will) Wraps the result of expr into the appropriate Value, eg.:
    ///     Value::Arr({ has: Type::Num, items: expr })
    fn as_value<T>(&self, expr: T) -> Toks where T: Iterator<Item=TokenTree> {
        tts!(
            parse(&self.plain_name), parents(expr)
        ).collect()
    }

    /// (will) Match (pattern before the '=>'), storing the result into the ident, eg.:
    ///     Value::Arr(ident)
    fn as_match(&self, ident: &str) -> String {
        format!("Some({}({ident})),", self.plain_name)
    }
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
                    if Spacing::Alone == p.spacing() { panic!("expected token '->'"); }
                    false
                } else { true }
            })
            .collect();

        match iter.next() {
            Some(TokenTree::Punct(p))
                if ':' == p.as_char()
                && Spacing::Alone == p.spacing() => panic!("expected token '->'"),
            _ => (),
        }

        let tail: Toks = iter.collect();

        [Param::new(head)]
            .into_iter()
            .chain(from_dsl_ty(tail))
            .collect()
    }
}

// wrap fn_tail around the match that extracts args (as well as the call)
fn wrap_extract_call<T>(params_and_ret: Vec<Param>, fn_tail: T) -> Toks where T: Iterator<Item=TokenTree> {
    let mut params = params_and_ret;
    let ret_last = params.pop().unwrap();

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
        .map(|(k, p)| parse(&format!("Some({}),", p.as_match(&format!("args{k}")))))
        .flatten());

    // TODO: wrap result in a Value::[..]
    ret_last.as_value(tts!(
        braces(tts!(
            parse("let mut args_iter = this.args.into_iter();"),
            parse("match"), parents(nexts), braces(tts!(
                match_pat, parse("=>"), parents(fn_tail), parents(arg_n),
                parse(", _ => unreachable!()"),
            ))
        ))
    ))
}

fn function<T>(name_ident: Ident, params_and_ret: Vec<Param>, fn_tail: T, iter_n: usize) -> Toks where T: Iterator<Item=TokenTree> {
    let (name, args) = if 0 == iter_n {
        ( tts!(parse(&format!("\"{name_ident}\".to_string()")))
        , tts!(parse("vec![]"))
        )
    } else {
        ( tts!(parse("this.name"))
        , tts!(parse("this.args"))
        )
    };

    // TODO: build the new type (maps) by keeping as ident whichever should
    let (_maps, func) = if params_and_ret.len()-1 == iter_n {
        ( ()
        , group(tts!(parse("|this|"), wrap_extract_call(params_and_ret, fn_tail)))
        )
    } else {
        ( ()                                      // TODO: v- will be receiving an updated version
        , group(tts!(parse("|this|"), function(name_ident, params_and_ret, fn_tail, iter_n+1)))
        )
    };

    tts!(
        parse("Value::Fun"), parents(tts!(
            parse("Function"), braces(tts!(
                parse("  name:"), name,
                parse(", maps:"), parents(tts!()),
                parse(", args:"), args,
                parse(", func:"), func,
            ))
        ))
    ).collect()
}

fn value<T>(_name_ident: Ident, ret_last: Param, val_tail: T) -> Toks where T: Iterator<Item=TokenTree> {
    ret_last.as_value(val_tail)
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
            => panic!("expected token '::'"),
        _ => (),
    }

    let ty: Toks = it
        .by_ref()
        .take_while(|t| "=" != t.to_string())
        .collect();
    if 0 == ty.len() { panic!("expected type expression (type)"); }

    let val: Toks = it.collect();
    if 0 == val.len() { panic!("expected token '=' then expression (value)"); }

    let types = from_dsl_ty(ty);
    // let ret = types.pop().unwrap();

    if 1 == types.len() {
        tts!(value(name, types.into_iter().next().unwrap(), tts!(val))).collect()
    } else {
        tts!(function(name, types, tts!(val), 0)).collect()
    }
}
