/// XXX: everything is pain, but can't be bothered; this will do for now (and maybe more)

extern crate proc_macro;
use proc_macro::{TokenStream, TokenTree, Ident, Span, Punct, Spacing, Group, Delimiter, Literal};

macro_rules! tts {
    ($h:expr, $($t:expr),+) => { tts!($h)$(.chain(tts!($t)))+ };
    ($h:expr, $($t:expr),+,) => { tts!($h)$(.chain(tts!($t)))+ };
    ($text:literal) => { $text.parse::<TokenStream>().unwrap().into_iter() };
    ($expr:expr) => { $expr.into_iter() };
    ($text:literal,) => { tts!($text) };
    ($expr:expr,) => { tts!($expr) };
    () => { tts!(vec![]) }
}
type Toks = Vec<TokenTree>;

fn ident(name: &str) -> Toks {
    vec![TokenTree::Ident(Ident::new(name, Span::call_site()))]
}
fn lit(v: Literal) -> Toks {
    vec![TokenTree::Literal(v)]
}
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
fn parents<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
    vec![TokenTree::Group(Group::new(Delimiter::Parenthesis, content.collect()))]
}
fn braces<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
    vec![TokenTree::Group(Group::new(Delimiter::Brace, content.collect()))]
}
fn bracks<T>(content: T) -> Toks where T: Iterator<Item=TokenTree> {
    vec![TokenTree::Group(Group::new(Delimiter::Bracket, content.collect()))]
}

fn function<T1, T2, T3, T4>(name: T1, maps: T2, args: T3, func: T4) -> Toks
    where
        T1: Iterator<Item=TokenTree>,
        T2: Iterator<Item=TokenTree>,
        T3: Iterator<Item=TokenTree>,
        T4: Iterator<Item=TokenTree>,
{
    tts!(ident("Value"), punct("::"), ident("Fun"), parents(tts!(
        ident("Function"), braces(tts!(
            ident("name"), punct(":"), name, punct(","),
            ident("maps"), punct(":"), maps, punct(","),
            ident("args"), punct(":"), args, punct(","),
            ident("func"), punct(":"), func, punct(","),
        ))
    ))).collect()
}

#[proc_macro]
pub fn val(stream: TokenStream) -> TokenStream {
    let mut it = stream.into_iter();

    let name = match it.next() {
        Some(TokenTree::Ident(a)) => a,
        _ => panic!("expected identifier (name)"),
    };
    println!("name: {:?}", name);

    match (it.next(), it.next()) {
        (Some(TokenTree::Punct(a)), Some(TokenTree::Punct(b))) if ':' == a.as_char() && ':' == b.as_char() && a.spacing() != b.spacing() => (),
        _ => panic!("expected token ::"),
    }

    let ty: Toks = it.by_ref().take_while(|t| "=" != t.to_string()).collect();
    println!("ty: {:?}", ty);

    let val: Toks = it.collect();
    println!("val: {:?}", val);


    tts!(function(
        tts!(lit(Literal::string(&name.to_string())), punct("."), ident("to_string"), parents(tts!())),
        tts!(parents(tts!())),
        tts!(parents(tts!())),
        tts!(val),
    )).collect()
}
