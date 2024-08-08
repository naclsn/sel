use std::fmt::{Display, Formatter, Result as FmtResult};
use std::iter;

use crate::builtin::lookup_type;

// principal public types (and consts) {{{
pub const COMPOSE_OP_FUNC_NAME: &str = "(,)";
const NUMBER_TYPEREF: usize = 0;
const STRFIN_TYPEREF: usize = 1;
const FINITE_TYPEREF: usize = 2;

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
pub enum Tree {
    Bytes(Vec<u8>),
    Number(i32),
    List(TypeRef, Vec<Tree>),
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
enum Type {
    Number,
    Bytes(Boundedness),
    List(Boundedness, TypeRef),
    Func(TypeRef, TypeRef),
    Named(String),
    Finite(bool),
}

#[derive(PartialEq, Debug, Clone)]
pub enum FrozenType {
    Number,
    Bytes(bool),
    List(bool, Box<FrozenType>),
    Func(Box<FrozenType>, Box<FrozenType>),
    Named(String),
}
// }}}

// lexing into tokens {{{
struct Lexer<I: Iterator<Item = u8>> {
    stream: iter::Peekable<I>,
}

impl<I: Iterator<Item = u8>> Lexer<I> {
    fn new(iter: I) -> Self {
        Self {
            stream: iter.peekable(),
        }
    }
}

impl<I: Iterator<Item = u8>> Iterator for Lexer<I> {
    type Item = Token;

    fn next(&mut self) -> Option<Self::Item> {
        Some(match self.stream.find(|c| !c.is_ascii_whitespace())? {
            b':' => Token::Bytes({
                iter::from_fn(|| match (self.stream.next(), self.stream.peek()) {
                    (Some(b':'), Some(b':')) => {
                        let _ = self.stream.next();
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

            b'#' => self
                .stream
                .find(|&c| b'\n' == c)
                .and_then(|_| self.next())?,

            c if c.is_ascii_lowercase() => Token::Word(
                String::from_utf8(
                    iter::once(c)
                        .chain(iter::from_fn(|| {
                            self.stream.next_if(u8::is_ascii_lowercase)
                        }))
                        .collect(),
                )
                .unwrap(),
            ),

            c if c.is_ascii_digit() => Token::Number({
                let mut r = 0i32;
                let (shift, digits) = match (c, self.stream.peek()) {
                    (b'0', Some(b'B' | b'b')) => {
                        self.stream.next();
                        (1, b"01".as_slice())
                    }
                    (b'0', Some(b'O' | b'o')) => {
                        self.stream.next();
                        (3, b"01234567".as_slice())
                    }
                    (b'0', Some(b'X' | b'x')) => {
                        self.stream.next();
                        (4, b"0123456789abcdef".as_slice())
                    }
                    _ => {
                        r = c as i32 - 48;
                        (0, b"0123456789".as_slice())
                    }
                };
                while let Some(k) = self
                    .stream
                    .peek()
                    .and_then(|&c| digits.iter().position(|&k| k == c | 32).map(|k| k as i32))
                {
                    self.stream.next();
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
                        .chain(
                            self.stream
                                .by_ref()
                                .take_while(|c| !c.is_ascii_whitespace()),
                        )
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
            Some(Token::Bytes(b)) => Ok(Tree::Bytes(b)),
            Some(Token::Number(n)) => Ok(Tree::Number(n)),

            Some(Token::OpenBracket) => {
                self.parse_script()
                    .and_then(|rr| match self.peekable.next() {
                        Some(Token::CloseBracket) => Ok(rr),
                        Some(token) => Err(Error::Unexpected(token)),
                        None => Err(Error::UnexpectedEnd),
                    })
            }

            Some(Token::OpenBrace) => {
                let mut items = Vec::new();
                let ty = self.types.named("a");
                if let Some(Token::CloseBrace) = self.peekable.peek() {
                    drop(self.peekable.next());
                } else {
                    loop {
                        let item = self.parse_value()?;
                        Type::concretize(ty, item.get_type(), &mut self.types)?;
                        items.push(item);
                        match self.peekable.next() {
                            Some(Token::Comma) => {
                                if let Some(Token::CloseBrace) = self.peekable.peek() {
                                    break;
                                }
                            }
                            Some(Token::CloseBrace) => break,
                            Some(token) => return Err(Error::Unexpected(token)),
                            None => return Err(Error::UnexpectedEnd),
                        }
                    }
                }
                Ok(Tree::List(self.types.list(self.types.finite(), ty), items))
            }

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
            // "unfold" means as in this example:
            // [tonum, add 1, tostr] :42:
            //  `-> (,)((,)(tonum, add(1)), tostr)
            // `-> (,)((,)(tonum(:[52, 50]:), add(1)), tostr)
            // => tostr(add(1, tonum(:[52, 50]:)))
            fn apply_maybe_unfold(
                p: &mut Parser<impl Iterator<Item = u8>>,
                func: Tree,
            ) -> Result<Tree, Error> {
                match func {
                    Tree::Apply(_, ref name, args) if COMPOSE_OP_FUNC_NAME == name => {
                        let mut args = args.into_iter();
                        let (f, g) = (args.next().unwrap(), args.next().unwrap());
                        // (,)(f, g) => g(f(..))
                        g.try_apply(apply_maybe_unfold(p, f)?, &mut p.types)
                    }
                    _ => func.try_apply(p.parse_value()?, &mut p.types),
                }
            }
            r = apply_maybe_unfold(self, r)?;
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
                        {
                            // (,) :: (a -> b) -> (b -> c) -> a -> c
                            let a = self.types.named("a");
                            let b = self.types.named("b");
                            let c = self.types.named("c");
                            let ab = self.types.func(a, b);
                            let bc = self.types.func(b, c);
                            let ac = self.types.func(a, c);
                            let ret = self.types.func(bc, ac);
                            self.types.func(ab, ret)
                        },
                        COMPOSE_OP_FUNC_NAME.into(),
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
}
// }}}

// types vec with holes {{{
#[derive(PartialEq, Debug, Clone)]
pub struct TypeList(Vec<Option<Type>>);
pub struct TypeRepr<'a>(TypeRef, &'a TypeList);

impl Default for TypeList {
    fn default() -> Self {
        TypeList(vec![
            Some(Type::Number),       // [NUMBER_TYPEREF]
            Some(Type::Bytes(2)),     // [STRFIN_TYPEREF]
            Some(Type::Finite(true)), // [FINITE_TYPEREF]
        ])
    }
}

impl TypeList {
    fn push(&mut self, it: Type) -> TypeRef {
        if let Some((k, o)) = self.0.iter_mut().enumerate().find(|(_, o)| o.is_none()) {
            *o = Some(it);
            k
        } else {
            self.0.push(Some(it));
            self.0.len() - 1
        }
    }

    pub fn number(&self) -> TypeRef {
        NUMBER_TYPEREF
    }
    pub fn bytes(&mut self, finite: Boundedness) -> TypeRef {
        if FINITE_TYPEREF == finite {
            STRFIN_TYPEREF
        } else {
            self.push(Type::Bytes(finite))
        }
    }
    pub fn list(&mut self, finite: Boundedness, item: TypeRef) -> TypeRef {
        self.push(Type::List(finite, item))
    }
    pub fn func(&mut self, par: TypeRef, ret: TypeRef) -> TypeRef {
        self.push(Type::Func(par, ret))
    }
    pub fn named(&mut self, name: &str) -> TypeRef {
        self.push(Type::Named(name.to_string()))
    }
    pub fn finite(&self) -> Boundedness {
        FINITE_TYPEREF
    }
    pub fn infinite(&mut self) -> Boundedness {
        self.push(Type::Finite(false))
    }

    fn get(&self, at: TypeRef) -> &Type {
        self.0[at].as_ref().unwrap()
    }

    fn get_mut(&mut self, at: TypeRef) -> &mut Type {
        self.0[at].as_mut().unwrap()
    }

    pub fn repr(&self, at: TypeRef) -> TypeRepr {
        TypeRepr(at, self)
    }

    #[allow(dead_code)]
    pub fn frozen(&self, at: TypeRef) -> FrozenType {
        match self.get(at) {
            Type::Number => FrozenType::Number,
            Type::Bytes(b) => FrozenType::Bytes(match self.get(*b) {
                Type::Finite(b) => *b,
                _ => unreachable!(),
            }),
            Type::List(b, items) => FrozenType::List(
                match self.get(*b) {
                    Type::Finite(b) => *b,
                    _ => unreachable!(),
                },
                Box::new(self.frozen(*items)),
            ),
            Type::Func(par, ret) => {
                FrozenType::Func(Box::new(self.frozen(*par)), Box::new(self.frozen(*ret)))
            }
            Type::Named(name) => FrozenType::Named(name.clone()),
            Type::Finite(_) => unreachable!(),
        }
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

    /// Essentially the same as `try_apply`, but doesn't concretize and doesn't fail.
    fn can_apply(&self, arg: &Tree, types: &TypeList) -> bool {
        Type::applicable(self.get_type(), arg.get_type(), types)
    }

    fn get_type(&self) -> TypeRef {
        match self {
            Tree::Bytes(_) => STRFIN_TYPEREF,
            Tree::Number(_) => NUMBER_TYPEREF,

            Tree::List(ty, _) => *ty,

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
            Tree::Bytes(v) => write!(f, ":{v:?}:"),
            Tree::Number(n) => write!(f, "{n}"),

            Tree::List(_, items) => {
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

// tests {{{
#[cfg(test)]
mod tests {
    use super::*;

    fn expect<T>(r: Result<T, Error>) -> T {
        r.unwrap_or_else(|e| match e {
            Error::Unexpected(t) => panic!("error: Unexpected token {t:?}"),
            Error::UnexpectedEnd => panic!("error: Unexpected end of script"),
            Error::UnknownName(n) => panic!("error: Unknown name '{n}'"),
            Error::NotFunc(o, types) => {
                panic!("error: Expected a function type, but got {}", types.repr(o))
            }
            Error::ExpectedButGot(w, g, types) => panic!(
                "error: Expected type {}, but got {}",
                types.repr(w),
                types.repr(g)
            ),
            Error::InfWhereFinExpected => {
                panic!("error: Expected finite type, but got infinite type")
            }
        })
    }

    fn compare(left: &Tree, right: &Tree) -> bool {
        match (left, right) {
            (Tree::List(_, la), Tree::List(_, ra)) => la
                .iter()
                .zip(ra.iter())
                .find(|(l, r)| !compare(l, r))
                .is_none(),
            (Tree::Apply(_, ln, la), Tree::Apply(_, rn, ra))
                if ln == rn && la.len() == ra.len() =>
            {
                la.iter()
                    .zip(ra.iter())
                    .find(|(l, r)| !compare(l, r))
                    .is_none()
            }
            (l, r) => *l == *r,
        }
    }

    macro_rules! assert_tree {
        ($left:expr, $right:expr) => {
            match (&$left, &$right) {
                (l, r) if !compare(l, r) => assert_eq!(l, r),
                _ => (),
            }
        };
    }

    #[test]
    fn lexing() {
        use Token::*;

        assert_eq!(
            &[Word("coucou".into()),][..],
            Lexer::new("coucou".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                OpenBracket,
                Word("a".into()),
                Number(1),
                Word("b".into()),
                Comma,
                OpenBrace,
                Word("w".into()),
                Comma,
                Word("x".into()),
                Comma,
                Word("y".into()),
                Comma,
                Word("z".into()),
                CloseBrace,
                CloseBracket,
            ][..],
            Lexer::new("[a 1 b, {w, x, y, z}]".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Bytes(vec![104, 97, 121]),
                Bytes(vec![104, 101, 121, 58, 32, 110, 111, 116, 32, 104, 97, 121]),
                Bytes(vec![]),
                Bytes(vec![58]),
                Word("fin".into()),
            ][..],
            Lexer::new(":hay: :hey:: not hay: :: :::: fin".bytes()).collect::<Vec<_>>()
        );

        assert_eq!(
            &[
                Number(42),
                Bytes(vec![42]),
                Number(42),
                Number(42),
                Number(42),
            ][..],
            Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn parsing() {
        use Tree::*;

        let (ty, types, res) = expect(Tree::new_typed("input".bytes()));
        assert_tree!(Apply(0, "input".into(), vec![]), res);
        assert_eq!(FrozenType::Bytes(false), types.frozen(ty));

        let (ty, types, res) = expect(Tree::new_typed(
            "input, split:-:, map[tonum, add1, tostr], join:+:".bytes(),
        ));
        assert_tree!(
            Apply(
                0,
                "join".into(),
                vec![
                    Bytes(vec![43]),
                    Apply(
                        0,
                        "map".into(),
                        vec![
                            Apply(
                                0,
                                COMPOSE_OP_FUNC_NAME.into(),
                                vec![
                                    Apply(
                                        0,
                                        COMPOSE_OP_FUNC_NAME.into(),
                                        vec![
                                            Apply(0, "tonum".into(), vec![]),
                                            Apply(0, "add".into(), vec![Number(1)])
                                        ]
                                    ),
                                    Apply(0, "tostr".into(), vec![])
                                ]
                            ),
                            Apply(
                                0,
                                "split".into(),
                                vec![Bytes(vec![45]), Apply(0, "input".into(), vec![]),],
                            ),
                        ],
                    ),
                ],
            ),
            res
        );
        assert_eq!(
            FrozenType::Bytes(true /* FIXME: this should be false, right? */),
            types.frozen(ty)
        );

        let (ty, types, res) = expect(Tree::new_typed("tonum, add234121, tostr, ln".bytes()));
        assert_tree!(
            Apply(
                0,
                COMPOSE_OP_FUNC_NAME.into(),
                vec![
                    Apply(
                        0,
                        COMPOSE_OP_FUNC_NAME.into(),
                        vec![
                            Apply(
                                0,
                                COMPOSE_OP_FUNC_NAME.into(),
                                vec![
                                    Apply(0, "tonum".into(), vec![]),
                                    Apply(0, "add".into(), vec![Number(234121)])
                                ]
                            ),
                            Apply(0, "tostr".into(), vec![])
                        ]
                    ),
                    Apply(0, "ln".into(), vec![])
                ]
            ),
            res
        );
        assert_eq!(
            FrozenType::Func(
                Box::new(FrozenType::Bytes(false)),
                Box::new(FrozenType::Bytes(true))
            ),
            types.frozen(ty)
        );

        let (ty, types, res) = expect(Tree::new_typed("[tonum, add234121, tostr] :13242:".bytes()));
        assert_tree!(
            Apply(
                0,
                "tostr".into(),
                vec![Apply(
                    0,
                    "add".into(),
                    vec![
                        Number(234121),
                        Apply(0, "tonum".into(), vec![Bytes(vec![49, 51, 50, 52, 50])])
                    ]
                )]
            ),
            res
        );
        assert_eq!(FrozenType::Bytes(true), types.frozen(ty));
    }
}
// }}}
