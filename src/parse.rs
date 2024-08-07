use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter;

use crate::builtin::lookup_type;

#[derive(PartialEq, Debug)]
pub enum Token {
    Unknown(String),
    Word(String),
    Bytes(Vec<u8>),
    Number(i32),
    Comma,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,
}

#[derive(PartialEq, Debug)]
pub enum TreeLeaf {
    Bytes(Vec<u8>),
    Number(i32),
}

#[derive(PartialEq, Debug)]
pub enum Tree {
    Atom(TreeLeaf),
    List(Vec<Tree>),
    Apply(TypeRef, String, Vec<Tree>),
}

#[derive(PartialEq, Debug)]
pub enum Error {
    Unexpected(Token),
    UnexpectedEnd,
    UnknownName(String),
    NotFunc(TypeRef, TypeList),
    ExpectedButGot(TypeRef, TypeRef, TypeList),
    InfWhereFinExpected,
}

pub type TypeRef = usize;
pub type Boundedness = usize;
#[derive(PartialEq, Debug, Clone)]
pub enum Type {
    Number,
    Bytes(Boundedness),
    List(Boundedness, TypeRef),
    Func(TypeRef, TypeRef),
    Named(String),
    Finite(bool),
}

// lexing into tokens {{{
struct Lexer<I: Iterator<Item = u8>>(iter::Peekable<I>);

impl<I: Iterator<Item = u8>> Lexer<I> {
    fn new(iter: I) -> Self {
        Self(iter.peekable())
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        Some(match self.0.find(|c| !c.is_ascii_whitespace())? {
            b':' => Token::Bytes({
                iter::from_fn(|| match (self.0.next(), self.0.peek()) {
                    (Some(b':'), Some(b':')) => {
                        let _ = self.0.next();
                        Some(b':')
                    }
                    (Some(b':'), _) => None,
                    (c, _) => c,
                })
                .collect()
            }),

            b',' => Token::Comma,
            b'[' => Token::OpenBracket,
            b']' => Token::CloseBracket,
            b'{' => Token::OpenBrace,
            b'}' => Token::CloseBrace,

            b'#' => self.0.find(|&c| b'\n' == c).and_then(|_| self.next())?,

            c if c.is_ascii_lowercase() => Token::Word(
                String::from_utf8(
                    iter::once(c)
                        .chain(iter::from_fn(|| self.0.next_if(u8::is_ascii_lowercase)))
                        .collect(),
                )
                .unwrap(),
            ),

            c if c.is_ascii_digit() => Token::Number({
                let mut r = 0i32;
                let (shift, digits) = match (c, self.0.peek()) {
                    (b'0', Some(b'B' | b'b')) => {
                        self.0.next();
                        (1, b"01".as_slice())
                    }
                    (b'0', Some(b'O' | b'o')) => {
                        self.0.next();
                        (3, b"01234567".as_slice())
                    }
                    (b'0', Some(b'X' | b'x')) => {
                        self.0.next();
                        (4, b"0123456789abcdef".as_slice())
                    }
                    _ => {
                        r = c as i32 - 48;
                        (0, b"0123456789".as_slice())
                    }
                };
                while let Some(k) = self
                    .0
                    .peek()
                    .and_then(|&c| digits.iter().position(|&k| k == c | 32).map(|k| k as i32))
                {
                    self.0.next();
                    r = if 0 == shift {
                        r * 10 + k
                    } else {
                        r << shift | k
                    }
                }
                r
            }),

            c => Token::Unknown(
                String::from_utf8_lossy(
                    &iter::once(c)
                        .chain(self.0.by_ref().take_while(|c| !c.is_ascii_whitespace()))
                        .collect::<Vec<_>>(),
                )
                .into_owned(),
            ),
        })
    }
}
// }}}

// parsing into tree {{{
struct Parser<I: Iterator<Item = u8>> {
    peekable: iter::Peekable<Lexer<I>>,
    types: TypeList,
}

impl<I: Iterator<Item = u8>> Parser<I> {
    fn new(iter: I) -> Parser<I> {
        Parser {
            peekable: Lexer::new(iter.into_iter()).peekable(),
            types: TypeList::default(),
        }
    }

    fn parse_value(&mut self) -> Result<Tree, Error> {
        match self.peekable.next() {
            Some(Token::Word(w)) => Ok(Tree::Apply(
                lookup_type(&w, &mut self.types).ok_or_else(|| Error::UnknownName(w.clone()))?,
                w,
                Vec::new(),
            )),
            Some(Token::Bytes(b)) => Ok(Tree::Atom(TreeLeaf::Bytes(b))),
            Some(Token::Number(n)) => Ok(Tree::Atom(TreeLeaf::Number(n))),

            Some(Token::OpenBracket) => {
                self.parse_script()
                    .and_then(|rr| match self.peekable.next() {
                        Some(Token::CloseBracket) => Ok(rr),
                        Some(token) => Err(Error::Unexpected(token)),
                        None => Err(Error::UnexpectedEnd),
                    })
            }

            Some(Token::OpenBrace) => iter::once(self.parse_value())
                .chain(iter::from_fn(|| match self.peekable.next() {
                    Some(Token::Comma) => Some(self.parse_value()),
                    Some(Token::CloseBrace) => None,
                    Some(token) => Some(Err(Error::Unexpected(token))),
                    None => Some(Err(Error::UnexpectedEnd)),
                }))
                .collect::<Result<_, _>>()
                .map(Tree::List),

            Some(token) => Err(Error::Unexpected(token)),
            None => Err(Error::UnexpectedEnd),
        }
    }

    fn parse_apply(&mut self) -> Result<Tree, Error> {
        let mut r = self.parse_value()?;
        while !matches!(
            self.peekable.peek(),
            Some(Token::Comma | Token::CloseBracket | Token::CloseBrace) | None
        ) {
            match r {
                Tree::Apply(_, ref name, args) if "(,)" == name => {
                    // [tonum, add 1, tostr] :42:
                    //  `-> (,)((,)(tonum, add(1)), tostr)
                    // `-> (,)((,)(tonum(:[52, 50]:), add(1)), tostr)
                    // => tostr(add(1, tonum(:[52, 50]:)))
                    //
                    // r is Apply(
                    //   "(,)",
                    //   [
                    //     Apply(
                    //       "(,)",
                    //       [
                    //         tonum,    <--- ref func
                    //         add1,
                    //       ]
                    //     ),
                    //     tostr
                    //   ]
                    // )

                    // (,)((,)(tonum, add(1)), tostr)
                    // f = (,)(tonum, add(1))   g = tostr

                    // TODO: there is probably a way to 'lift' bidoof a level above
                    // and remove the surrounding match given how similar they are
                    let mut args = args.into_iter();
                    let (f, g) = (args.next().unwrap(), args.next().unwrap());
                    r = self.bidoof(f, g)?;
                }
                _ => r = r.try_apply(self.parse_value()?, &mut self.types)?,
            }
        }
        Ok(r)
    }

    fn parse_script(&mut self) -> Result<Tree, Error> {
        let mut r = self.parse_apply()?;
        if let Some(Token::Comma) = self.peekable.peek() {
            drop(self.peekable.next());
            let mut then = self.parse_apply()?;

            // [x, f, g, h] :: d; where
            // - x :: A
            // - f, g, h :: a -> b, b -> c, c -> d
            if then.can_apply(&r, &self.types) {
                while let Some(Token::Comma) = {
                    r = then.try_apply(r, &mut self.types)?;
                    self.peekable.peek()
                } {
                    drop(self.peekable.next());
                    then = self.parse_apply()?;
                }
            }
            // [f, g, h] :: a -> d; where
            // - f, g, h :: a -> b, b -> c, c -> d
            else {
                while let Some(Token::Comma) = {
                    r = Tree::Apply(
                        // TODO: maybe inline its type def
                        lookup_type("(,)", &mut self.types).unwrap(),
                        "(,)".into(),
                        Vec::new(),
                    )
                    .try_apply(r, &mut self.types)?
                    .try_apply(then, &mut self.types)?;
                    self.peekable.peek()
                } {
                    drop(self.peekable.next());
                    then = self.parse_apply()?;
                }
            }
        }
        Ok(r)
    }

    // (TODO: name)
    fn bidoof(&mut self, f: Tree, g: Tree) -> Result<Tree, Error> {
        g.try_apply(
            match f {
                // (,)( (,)(h, i) , g) => g(bidoof(h, i)) => g(i(h(..)))
                Tree::Apply(_, ref name, args) if "(,)" == name => {
                    let mut args = args.into_iter();
                    let (h, i) = (args.next().unwrap(), args.next().unwrap());
                    self.bidoof(h, i)
                }
                // (,)(f, g) => g(f(..))
                _ => f.try_apply(self.parse_value()?, &mut self.types),
            }?,
            &mut self.types,
        )
    }
}
// }}}

// types vec with holes {{{
#[derive(PartialEq, Debug, Clone)]
pub struct TypeList(Vec<Option<Type>>);
pub struct TypeRepr<'a>(TypeRef, &'a TypeList);

impl Default for TypeList {
    fn default() -> Self {
        TypeList(vec![
            Some(Type::Number),
            Some(Type::Bytes(2)),
            Some(Type::Finite(true)),
        ])
    }
}

impl TypeList {
    pub fn push(&mut self, it: Type) -> TypeRef {
        if let Some((k, o)) = self.0.iter_mut().enumerate().find(|(_, o)| o.is_none()) {
            *o = Some(it);
            k
        } else {
            self.0.push(Some(it));
            self.0.len() - 1
        }
    }

    pub fn get(&self, at: TypeRef) -> &Type {
        self.0[at].as_ref().unwrap()
    }

    pub fn get_mut(&mut self, at: TypeRef) -> &mut Type {
        self.0[at].as_mut().unwrap()
    }

    pub fn repr(&self, at: TypeRef) -> TypeRepr {
        TypeRepr(at, self)
    }

    //pub fn pop(&mut self, at: TypeRef) -> Type {
    //    let r = self.0[at].take().unwrap();
    //    while let Some(None) = self.0.last() {
    //        self.0.pop();
    //    }
    //    r
    //}
}
// }}}

// type apply and concretize {{{
impl Type {
    /// `func give` so returns type of `func` (if checks out)
    fn applied(func: TypeRef, give: TypeRef, types: &mut TypeList) -> Result<TypeRef, Error> {
        if let &Type::Func(want, ret) = types.get(func) {
            // want(a, b..) <- give(A, b..)
            Type::concretize(want, give, types).map(|()| ret)
        } else {
            Err(Error::NotFunc(func, types.clone()))
        }
    }

    /// Concretizes boundedness and unknown named types:
    /// * fin <- fin is fin
    /// * inf <- inf is inf
    /// * inf <- fin is fin
    /// * unk <- Knw is Knw
    ///
    /// Both types may be modified (due to contravariance of func in parameter type).
    /// This uses the fact that inf types and unk named types are
    /// only depended on by the functions that introduced them.
    fn concretize(want: TypeRef, give: TypeRef, types: &mut TypeList) -> Result<(), Error> {
        match (types.get(want), types.get(give)) {
            (Type::Number, Type::Number) => Ok(()),

            (Type::Bytes(fw), Type::Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Type::Finite(fw_bool), Type::Finite(fg_bool)) if fw_bool == fg_bool => Ok(()),
                (Type::Finite(false), Type::Finite(true)) => {
                    *types.get_mut(*fw) = Type::Finite(true);
                    Ok(())
                }
                (Type::Finite(true), Type::Finite(false)) => Err(Error::InfWhereFinExpected),
                _ => unreachable!(),
            },

            (Type::List(fw, l_ty), Type::List(fg, r_ty)) => {
                let (l_ty, r_ty) = (*l_ty, *r_ty);
                match (types.get(*fw), types.get(*fg)) {
                    (Type::Finite(fw_bool), Type::Finite(fg_bool)) if fw_bool == fg_bool => (),
                    (Type::Finite(false), Type::Finite(true)) => {
                        *types.get_mut(*fw) = Type::Finite(true)
                    }
                    (Type::Finite(true), Type::Finite(false)) => {
                        return Err(Error::InfWhereFinExpected)
                    }
                    _ => unreachable!(),
                }
                Type::concretize(l_ty, r_ty, types)
            }

            (&Type::Func(l_par, l_ret), &Type::Func(r_par, r_ret)) => {
                // (a -> b) <- (Str -> Num)
                // parameter compare contravariantly:
                // (Str -> c) <- (Str* -> Str*)
                // (a -> b) <- (c -> c)
                Type::concretize(r_par, l_par, types)?;
                Type::concretize(l_ret, r_ret, types)
            }

            (other, Type::Named(_)) => {
                *types.get_mut(give) = other.clone();
                Ok(())
            }
            (Type::Named(_), other) => {
                *types.get_mut(want) = other.clone();
                Ok(())
            }

            _ => Err(Error::ExpectedButGot(want, give, types.clone())),
        }
    }

    fn applicable(func: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        if let &Type::Func(want, _) = types.get(func) {
            Type::compatible(want, give, types)
        } else {
            false
        }
    }

    fn compatible(want: TypeRef, give: TypeRef, types: &TypeList) -> bool {
        match (types.get(want), types.get(give)) {
            (Type::Number, Type::Number) => true,

            (Type::Bytes(fw), Type::Bytes(fg)) => match (types.get(*fw), types.get(*fg)) {
                (Type::Finite(true), Type::Finite(false)) => false,
                (Type::Finite(_), Type::Finite(_)) => true,
                _ => unreachable!(),
            },

            (Type::List(fw, l_ty), Type::List(fg, r_ty)) => {
                match (types.get(*fw), types.get(*fg)) {
                    (Type::Finite(true), Type::Finite(false)) => false,
                    (Type::Finite(_), Type::Finite(_)) => Type::compatible(*l_ty, *r_ty, types),
                    _ => unreachable!(),
                }
            }

            (Type::Func(l_par, l_ret), Type::Func(r_par, r_ret)) => {
                // parameter compare contravariantly: (Str -> c) <- (Str* -> Str*)
                Type::compatible(*r_par, *l_par, types) && Type::compatible(*l_ret, *r_ret, types)
            }

            (_, Type::Named(_)) => true,
            (Type::Named(_), _) => true,

            _ => false,
        }
    }
}
// }}}

// uuhh.. tree- the entry point (Tree::new) {{{
impl Tree {
    fn try_apply(self, arg: Tree, types: &mut TypeList) -> Result<Tree, Error> {
        let ty = Type::applied(self.get_type(), arg.get_type(), types)?;
        let Tree::Apply(_, name, mut args) = self else {
            unreachable!();
        };
        args.push(arg);
        Ok(Tree::Apply(ty, name, args))
    }

    /// Essentially the same as `try_apply`, but doesn't concretize.
    /// Mutates `self` and `arg` (and `types`) to type them;
    /// such as when it was `Atom(Word(name))`, it becomes `Apply(ty, name, vec![])`.
    /// It is okay to do so because this will have to be done anyways
    /// and as it is it won't do it multiple times needlessly.
    fn can_apply(&self, arg: &Tree, types: &TypeList) -> bool {
        Type::applicable(self.get_type(), arg.get_type(), types)
    }

    fn get_type(&self) -> TypeRef {
        match self {
            Tree::Atom(atom) => match atom {
                TreeLeaf::Bytes(_) => 1,
                TreeLeaf::Number(_) => 0,
            },

            Tree::List(_items) => todo!(),

            Tree::Apply(ty, _, _) => *ty,
        }
    }

    #[allow(dead_code)]
    pub fn new(iter: impl IntoIterator<Item = u8>) -> Result<Tree, Error> {
        Parser::new(iter.into_iter()).parse_script()
    }

    #[allow(dead_code)]
    pub fn new_typed(
        iter: impl IntoIterator<Item = u8>,
    ) -> Result<(TypeRef, TypeList, Tree), Error> {
        let mut p = Parser::new(iter.into_iter());
        p.parse_script().map(|r| (r.get_type(), p.types, r))
    }
}
// }}}

// display impls {{{
impl Display for TypeRepr<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        let types = &self.1;
        match types.get(self.0) {
            Type::Number => write!(f, "Num"),

            Type::Bytes(b) => write!(f, "Str{}", types.repr(*b)),

            Type::List(b, has) => write!(f, "[{}]{}", types.repr(*has), types.repr(*b)),

            Type::Func(par, ret) => {
                let funcarg = matches!(types.get(*par), Type::Func(_, _));
                if funcarg {
                    write!(f, "(")?;
                }
                write!(f, "{}", types.repr(*par))?;
                if funcarg {
                    write!(f, ")")?;
                }
                write!(f, " -> ")?;
                write!(f, "{}", types.repr(*ret))
            }

            Type::Named(name) => write!(f, "{name}"),

            Type::Finite(true) => Ok(()),
            Type::Finite(false) => write!(f, "*"),
        }
    }
}

impl Display for Tree {
    fn fmt(&self, f: &mut Formatter<'_>) -> FmtResult {
        match self {
            Tree::Atom(atom) => match atom {
                TreeLeaf::Bytes(v) => write!(f, ":{v:?}:"),
                TreeLeaf::Number(n) => write!(f, "{n}"),
            },

            Tree::List(items) => {
                write!(f, "{{")?;
                let mut sep = "";
                for it in items {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, "}}")
            }

            Tree::Apply(_, w, empty) if empty.is_empty() => write!(f, "{w}"),
            Tree::Apply(_, name, args) => {
                write!(f, "{name}(")?;
                let mut sep = "";
                for it in args {
                    write!(f, "{sep}{it}")?;
                    sep = ", ";
                }
                write!(f, ")")
            }
        }
    }
}
// }}}

/*
// tests {{{
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lexing() {
        assert_eq!(
            &[Token::Word("coucou".into()),][..],
            Lexer::new("coucou".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token::OpenBracket,
                Token::Word("a".to_string()),
                Token::Number(1),
                Token::Word("b".to_string()),
                Token::Comma,
                Token::OpenBrace,
                Token::Word("w".to_string()),
                Token::Comma,
                Token::Word("x".to_string()),
                Token::Comma,
                Token::Word("y".to_string()),
                Token::Comma,
                Token::Word("z".to_string()),
                Token::CloseBrace,
                Token::CloseBracket,
            ][..],
            Lexer::new("[a 1 b, {w, x, y, z}]".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Token::Number(42),
                Token::Bytes(vec![42]),
                Token::Number(42),
                Token::Number(42),
                Token::Number(42),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn parsing() {
        assert_eq!(
            Ok(Tree::Atom(TreeLeaf::Word("coucou".to_string()))),
            Tree::new("coucou".bytes(), &mut TypeList::new())
        );

        assert_eq!(
            Ok(Tree::Apply(
                0,
                "join".to_string(),
                vec![
                    Tree::Atom(TreeLeaf::Bytes(vec![43])),
                    Tree::Apply(
                        0,
                        "map".to_string(),
                        vec![
                            Tree::Apply(
                                0,
                                "add".to_string(),
                                vec![Tree::Atom(TreeLeaf::Number(1))],
                            ),
                            Tree::Apply(
                                0,
                                "split".to_string(),
                                vec![
                                    Tree::Atom(TreeLeaf::Bytes(vec![45])),
                                    Tree::Atom(TreeLeaf::Word("input".to_string())),
                                ],
                            ),
                        ],
                    ),
                ],
            )),
            Tree::new(
                "input, split:-:, map[add1], join:+:".bytes(),
                &mut TypeList::new()
            )
        );

        // todo
        //assert_eq!(Err...)
    }

    #[test]
    fn typing() {
        // todo
    }
}
// }}}
*/