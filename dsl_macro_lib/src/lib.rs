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
#[derive(Debug, Clone)]
enum Type {
    Num,
    Str,
    Arr(Box<Type>),
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
    fn new_atom<T>(mut source: T) -> Self where T: Iterator<Item=TokenTree> {
        match source.next() {

            Some(TokenTree::Ident(i)) => {
                match i.to_string().as_str() {
                    "Num" => Type::Num,
                    "Str" => Type::Str,
                    name => Type::Unk(name.to_string()),
                }
            },

            Some(TokenTree::Group(p)) if Delimiter::Bracket == p.delimiter() => {
                Type::Arr(Box::new(Type::new_atom(source))) // TODO: take between brackets
            },

            Some(TokenTree::Group(p)) if Delimiter::Parenthesis == p.delimiter() => {
                // TODO: take between parenthesis
                let types = from_dsl_ty(source.collect());
                let mut iter = types.into_iter().rev();
                let first = iter.next().unwrap();
                iter.fold(first, |acc, cur|
                        Type::Fun(
                            Box::new(cur),
                            Box::new(acc)
                        ))
            },

            Some(other) => panic!("expected type atom, got '{other}'"),
            None => panic!("expecte type atom"),

        }
    }

    fn plain_name(&self) -> &str {
        match self {
            Type::Num => "Num",
            Type::Str => "Str",
            Type::Arr(_) => "Arr",
            Type::Fun(_, _) => "Fun",
            Type::Unk(_) => unreachable!("[internal] this is probably not supposed to get here, but not sure (plain_name for 'Unk')"),
            Type::Now(_) => unreachable!("[internal] this is probably not supposed to get here, but not sure (plain_name for 'Now')"),
        }
    }

    // TODO: replace occurences of `Unk(name)` with `Now(name)`
    // for every names in `known`
    fn now_known(self, known: Vec<String>) -> Type { todo!() }

    // TODO
    /// (will) Create the appropriate Type value, eg.:
    ///     Type::Arr(Type::Num)
    ///     Type::Unk("a".to_string())
    ///     a // when it's a now known type (then is has been update via now_known)
    fn as_type<T>(&self) -> Toks where T: Iterator<Item=TokenTree> {
        parse("Type::Num")
    }

    /// (will) Wraps the result of expr into the appropriate Value, eg.:
    ///     Value::Arr({ has: Type::Num, items: expr })
    fn as_value<T>(&self, expr: T) -> Toks where T: Iterator<Item=TokenTree> {
        match self {

            Type::Num | Type::Str =>
                tts!(
                    parse(&format!("Value::{}", self.plain_name())), parents(expr)
                ).collect(),

            // TODO
            Type::Arr(a) => todo!(), // Array { has: a, items: expr }
            Type::Fun(a, b) => todo!(), // Function {} // XXX: is it supposed to reach here?

            Type::Unk(name) => unreachable!("[internal] trying to produce a value of unknown type '{name}'"),
            Type::Now(name) => todo!("[internal] trying to produce a value of now-known type '{name}'"), // how that works for eg. `id`
        }
    }

    /// (will) Match (pattern before the '=>'), storing the result into the ident, eg.:
    ///     Value::Arr(ident)
    fn as_match(&self, ident: &str) -> String {
        format!("Some(Value::{}({ident})),", self.plain_name())
    }
}

fn from_dsl_ty(tts: Toks) -> Vec<Type> {
    if 0 == tts.len() {
        vec![]
    } else {
        let mut iter = tts.into_iter();

        let head: Toks = iter
            .by_ref()
            .take_while(|t| {
                if let TokenTree::Punct(p) = t {
                    if '-' != p.as_char()
                    || Spacing::Joint != p.spacing() { panic!("expected token '->', got {}", TokenTree::Punct(p.clone())); }
                    false
                } else { true }
            })
            .collect();
        if 0 == head.len() {
            match iter.next() {
                Some(other) => panic!("expected type atom, got '{other}'"),
                None => panic!("expected type atom"),
            }
        }

        match iter.next() {
            Some(TokenTree::Punct(p))
                if '>' != p.as_char()
                || Spacing::Alone != p.spacing() => panic!("expected token '->', got {}", TokenTree::Punct(p)),
            _ => (),
        }

        let tail: Toks = iter.collect();

        [Type::new_atom(head.into_iter())]
            .into_iter()
            .chain(from_dsl_ty(tail))
            .collect()
    }
}

// (will) Call fn_tail with and after extracting the (correctly typed) args
fn wrap_extract_call<T>(params_and_ret: Vec<Type>, fn_tail: T) -> Toks where T: Iterator<Item=TokenTree> {
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

    // XXX: hos that works exactly with eg. `id`?
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

fn function<T>(name_ident: Ident, params_and_ret: Vec<Type>, fn_tail: T, iter_n: usize) -> Toks where T: Iterator<Item=TokenTree> {
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
    // this will be done by constructing a new vec, mapping the previous on
    // through `Type::now_known` which means the list of known types has to
    // be build

    let func = if params_and_ret.len()-1 == iter_n {
        group(tts!(parse("|this|"), wrap_extract_call(params_and_ret, fn_tail)))
    } else {
                                                // TODO: v- will be receiving an updated version
        group(tts!(parse("|this|"), function(name_ident, params_and_ret, fn_tail, iter_n+1)))
    };

    tts!(
        parse("Value::Fun"), parents(tts!(
            parse("Function"), braces(tts!(
                parse("  name:"), name,
                parse(", maps:"), parents(tts!()), // maps,
                parse(", args:"), args,
                parse(", func:"), func,
            ))
        ))
    ).collect()
}

fn value<T>(_name_ident: Ident, ret_last: Type, val_tail: T) -> Toks where T: Iterator<Item=TokenTree> {
    ret_last.as_value(val_tail)
}

#[proc_macro]
pub fn val(stream: TokenStream) -> TokenStream {
    let mut it = stream.into_iter();

    let name = match it.next() {
        Some(TokenTree::Ident(a)) => a,
        Some(other) => panic!("expected identifier (name), got '{other}'"),
        None => panic!("expected identifier (name)"),
    };

    match (it.next(), it.next()) {
        (Some(TokenTree::Punct(a)), Some(TokenTree::Punct(b)))
            if ':' != a.as_char()
            || ':' != b.as_char()
            || a.spacing() == b.spacing()
            => match a.spacing() {
                Spacing::Joint => panic!("expected token '::', got '{}{}'", TokenTree::Punct(a), TokenTree::Punct(b)),
                Spacing::Alone => panic!("expected token '::', got '{}'", TokenTree::Punct(a)),
            },
        _ => (),
    }

    let ty: Toks = it
        .by_ref()
        .take_while(|t| "=" != t.to_string())
        .collect();
    if 0 == ty.len() {
        match it.next() {
            Some(other) => panic!("expected type expression (type), got '{other}'"),
            None => panic!("expected type expression (type)"),
        }
    }

    let val: Toks = it.collect();
    if 0 == val.len() { panic!("expected token '=' then expression (value)"); }

    let types = from_dsl_ty(ty);

    if 1 == types.len() {
        tts!(value(name, types.into_iter().next().unwrap(), tts!(val))).collect()
    } else {
        tts!(function(name, types, tts!(val), 0)).collect()
    }
}
