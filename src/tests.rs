use crate::error::{Error, ErrorContext, ErrorKind, ErrorList};
use crate::parse::{Lexer, Location, Token, TokenKind, Tree, TreeKind, COMPOSE_OP_FUNC_NAME};
use crate::types::{FrozenType, ERROR_TYPE_NAME};

// utils {{{
fn expect<T>(r: Result<T, ErrorList>) -> T {
    r.unwrap_or_else(|e| {
        e.crud_report();
        panic!()
    })
}

fn compare(left: &Tree, right: &Tree) -> bool {
    match (&left.1, &right.1) {
        (TreeKind::List(_, la), TreeKind::List(_, ra)) => la
            .iter()
            .zip(ra.iter())
            .find(|(l, r)| !compare(l, r))
            .is_none(),
        (TreeKind::Apply(_, ln, la), TreeKind::Apply(_, rn, ra))
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

macro_rules! assert_maytree {
    ($left:expr, $right:expr) => {
        match (&$left, &$right) {
            (Ok(l), Ok(r)) if !compare(l, r) => assert_eq!(l, r),
            (Ok(_), Ok(_)) => (),
            (l, r) => assert_eq!(l, r),
        }
    };
}
// }}}

// lexing {{{
#[test]
fn lexing() {
    use TokenKind::*;

    assert_eq!(
        Vec::<Token>::new(),
        Lexer::new("".bytes()).collect::<Vec<_>>()
    );

    assert_eq!(
        &[Token(Location(0), Word("coucou".into()))][..],
        Lexer::new("coucou".bytes()).collect::<Vec<_>>()
    );

    assert_eq!(
        &[
            Token(Location(0), OpenBracket),
            Token(Location(1), Word("a".into())),
            Token(Location(3), Number(1.0)),
            Token(Location(5), Word("b".into())),
            Token(Location(6), Comma),
            Token(Location(8), OpenBrace),
            Token(Location(9), Word("w".into())),
            Token(Location(10), Comma),
            Token(Location(12), Word("x".into())),
            Token(Location(13), Comma),
            Token(Location(15), Word("y".into())),
            Token(Location(16), Comma),
            Token(Location(18), Word("z".into())),
            Token(Location(19), CloseBrace),
            Token(Location(20), Comma),
            Token(Location(22), Number(0.5)),
            Token(Location(25), CloseBracket)
        ][..],
        Lexer::new("[a 1 b, {w, x, y, z}, 0.5]".bytes()).collect::<Vec<_>>()
    );

    assert_eq!(
        &[
            Token(Location(0), Bytes(vec![104, 97, 121])),
            Token(
                Location(6),
                Bytes(vec![104, 101, 121, 58, 32, 110, 111, 116, 32, 104, 97, 121])
            ),
            Token(Location(22), Bytes(vec![])),
            Token(Location(25), Bytes(vec![58])),
            Token(Location(30), Word("fin".into()))
        ][..],
        Lexer::new(":hay: :hey:: not hay: :: :::: fin".bytes()).collect::<Vec<_>>()
    );

    assert_eq!(
        &[
            Token(Location(0), Number(42.0)),
            Token(Location(3), Bytes(vec![42])),
            Token(Location(7), Number(42.0)),
            Token(Location(12), Number(42.0)),
            Token(Location(21), Number(42.0)),
        ][..],
        Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>()
    );
}
// }}}

// parsing {{{
#[test]
fn parsing() {
    use TreeKind::*;

    assert_eq!(
        Err({
            let mut el = ErrorList::new();
            el.push(Error(
                Location(0),
                ErrorKind::Unexpected {
                    token: TokenKind::End,
                    expected: "a value",
                },
            ));
            el
        }),
        Tree::new_typed("".bytes())
    );

    let (ty, res) = expect(Tree::new_typed("input".bytes()));
    assert_tree!(Tree(Location(0), Apply(0, "input".into(), vec![])), res);
    assert_eq!(FrozenType::Bytes(false), ty);

    let (ty, res) = expect(Tree::new_typed(
        "input, split:-:, map[tonum, add1, tostr], join:+:".bytes(),
    ));
    assert_tree!(
        Tree(
            Location(42),
            Apply(
                0,
                "join".into(),
                vec![
                    Tree(Location(46), Bytes(vec![43])),
                    Tree(
                        Location(17),
                        Apply(
                            0,
                            "map".into(),
                            vec![
                                Tree(
                                    Location(20),
                                    Apply(
                                        0,
                                        COMPOSE_OP_FUNC_NAME.into(),
                                        vec![
                                            Tree(
                                                Location(26),
                                                Apply(
                                                    0,
                                                    COMPOSE_OP_FUNC_NAME.into(),
                                                    vec![
                                                        Tree(
                                                            Location(21),
                                                            Apply(0, "tonum".into(), vec![])
                                                        ),
                                                        Tree(
                                                            Location(28),
                                                            Apply(
                                                                0,
                                                                "add".into(),
                                                                vec![Tree(
                                                                    Location(31),
                                                                    Number(1.0)
                                                                )]
                                                            )
                                                        )
                                                    ]
                                                )
                                            ),
                                            Tree(Location(34), Apply(0, "tostr".into(), vec![]))
                                        ]
                                    )
                                ),
                                Tree(
                                    Location(7),
                                    Apply(
                                        0,
                                        "split".into(),
                                        vec![
                                            Tree(Location(12), Bytes(vec![45])),
                                            Tree(Location(0), Apply(0, "input".into(), vec![]))
                                        ]
                                    )
                                )
                            ]
                        )
                    )
                ]
            )
        ),
        res
    );
    // FIXME: this should be false, right?
    assert_eq!(FrozenType::Bytes(true), ty);

    let (ty, res) = expect(Tree::new_typed("tonum, add234121, tostr, ln".bytes()));
    assert_tree!(
        Tree(
            Location(23),
            Apply(
                0,
                COMPOSE_OP_FUNC_NAME.into(),
                vec![
                    Tree(
                        Location(16),
                        Apply(
                            0,
                            COMPOSE_OP_FUNC_NAME.into(),
                            vec![
                                Tree(
                                    Location(5),
                                    Apply(
                                        0,
                                        COMPOSE_OP_FUNC_NAME.into(),
                                        vec![
                                            Tree(Location(0), Apply(0, "tonum".into(), vec![])),
                                            Tree(
                                                Location(7),
                                                Apply(
                                                    0,
                                                    "add".into(),
                                                    vec![Tree(Location(10), Number(234121.0))]
                                                )
                                            )
                                        ]
                                    )
                                ),
                                Tree(Location(18), Apply(0, "tostr".into(), vec![]))
                            ]
                        )
                    ),
                    Tree(Location(25), Apply(0, "ln".into(), vec![]))
                ]
            )
        ),
        res
    );
    assert_eq!(
        FrozenType::Func(
            Box::new(FrozenType::Bytes(false)),
            Box::new(FrozenType::Bytes(true))
        ),
        ty
    );

    let (ty, res) = expect(Tree::new_typed("[tonum, add234121, tostr] :13242:".bytes()));
    assert_tree!(
        Tree(
            Location(19),
            Apply(
                0,
                "tostr".into(),
                vec![Tree(
                    Location(8),
                    Apply(
                        0,
                        "add".into(),
                        vec![
                            Tree(Location(11), Number(234121.0)),
                            Tree(
                                Location(1),
                                Apply(
                                    0,
                                    "tonum".into(),
                                    vec![Tree(Location(26), Bytes(vec![49, 51, 50, 52, 50]))]
                                )
                            )
                        ]
                    )
                )]
            )
        ),
        res
    );
    assert_eq!(FrozenType::Bytes(true), ty);
}
// }}}

// reporting {{{
#[test]
fn reporting() {
    use ErrorContext::*;
    use ErrorKind::*;
    use FrozenType::*;

    assert_maytree!(
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(Location(10), ExpectedButGot(Number, Bytes(true)))),
                because: TypeListInferredItemType(Number)
            }
        )])),
        Tree::new("{1, 2, 3, :soleil:}".bytes())
    );

    assert_maytree!(
        Ok(Tree(
            Location(0),
            TreeKind::List(
                0,
                vec![
                    Tree(
                        Location(1),
                        TreeKind::Apply(
                            0,
                            "repeat".into(),
                            vec![Tree(Location(8), TreeKind::Number(1.0))]
                        )
                    ),
                    Tree(Location(11), TreeKind::List(0, vec![]))
                ]
            )
        )),
        Tree::new("{repeat 1, {}}".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(Location(5), InfWhereFinExpected)),
                because: TypeListInferredItemType(List(true, Box::new(Named("a".into()))))
            }
        )])),
        Tree::new("{{}, repeat 1}".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(Location(7), InfWhereFinExpected)),
                because: TypeListInferredItemType(List(true, Box::new(Number)))
            }
        )])),
        Tree::new("{{42}, repeat 1}".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![
            Error(
                Location(0),
                ContextCaused {
                    error: Box::new(Error(Location(10), ExpectedButGot(Number, Bytes(true)))),
                    because: TypeListInferredItemType(Number)
                }
            ),
            Error(
                Location(21),
                ContextCaused {
                    error: Box::new(Error(
                        Location(0),
                        ExpectedButGot(
                            Bytes(false),
                            List(true, Box::new(Named(ERROR_TYPE_NAME.into())))
                        )
                    )),
                    because: ChainedFromAsNthArgToNamedNowTyped(
                        Location(19),
                        1,
                        "ln".into(),
                        Func(Box::new(Bytes(false)), Box::new(Bytes(false)))
                    )
                }
            )
        ])),
        Tree::new("{1, 2, 3, :soleil:}, ln".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(Location(6), ExpectedButGot(Number, Bytes(true)))),
                because: AsNthArgToNamedNowTyped(
                    2,
                    "add".into(),
                    Func(Box::new(Number), Box::new(Number))
                )
            }
        )])),
        Tree::new("add 1 :2:".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![Error(
            Location(6),
            ContextCaused {
                error: Box::new(Error(Location(0), ExpectedButGot(Number, Bytes(true)))),
                because: ChainedFromAsNthArgToNamedNowTyped(
                    Location(4),
                    2,
                    "add".into(),
                    Func(Box::new(Number), Box::new(Number))
                )
            }
        )])),
        Tree::new(":42:, add 1".bytes())
    );

    assert_maytree!(
        Ok(Tree(
            Location(5),
            TreeKind::Apply(
                0,
                COMPOSE_OP_FUNC_NAME.into(),
                vec![
                    Tree(
                        Location(0),
                        TreeKind::Apply(
                            0,
                            "add".into(),
                            vec![Tree(Location(4), TreeKind::Number(1.0))]
                        )
                    ),
                    Tree(Location(7), TreeKind::Apply(0, "tostr".into(), vec![]))
                ]
            )
        )),
        Tree::new("add 1, tostr".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![Error(
            Location(7),
            ContextCaused {
                error: Box::new(Error(Location(0), ExpectedButGot(Bytes(false), Number))),
                because: ChainedFromAsNthArgToNamedNowTyped(
                    Location(5),
                    1,
                    "tonum".into(),
                    Func(Box::new(Bytes(false)), Box::new(Number))
                )
            }
        )])),
        Tree::new("add 1, tonum".bytes())
    );

    assert_maytree!(
        Err(ErrorList(vec![
            Error(Location(0), UnknownName("prout".into())),
            Error(Location(9), UnknownName("caca".into()))
        ])),
        Tree::new("prout 1, caca".bytes())
    );
}
// }}}
