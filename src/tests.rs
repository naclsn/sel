use crate::error::{Error, ErrorContext, ErrorKind, ErrorList};
use crate::parse::{Lexer, Location, Token, TokenKind, Tree, TreeKind, COMPOSE_OP_FUNC_NAME};
use crate::types::FrozenType;

// utils {{{
fn expect<T>(r: Result<T, ErrorList>, script: &str) -> T {
    r.unwrap_or_else(|e| {
        e.crud_report();
        panic!("{}", script)
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
    ($script:literal, $type:expr, $tree:expr) => {
        let (ty, res) = expect(Tree::new_typed($script.bytes()), $script);
        match (&$tree, &res) {
            (l, r) if !compare(l, r) => assert_eq!(l, r, "{}", $script),
            _ => assert_eq!($type, ty, "{}", $script),
        }
    };
}

macro_rules! assert_maytree {
    ($script:literal, $result:expr) => {
        match (&$result, &Tree::new($script.bytes())) {
            (Ok(l), Ok(r)) if !compare(l, r) => assert_eq!(l, r, "{}", $script),
            (Ok(_), Ok(_)) => (),
            (l, r) => assert_eq!(l, r, "{}", $script),
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
        Lexer::new("".bytes()).collect::<Vec<_>>(),
        ""
    );

    assert_eq!(
        &[Token(Location(0), Word("coucou".into()))][..],
        Lexer::new("coucou".bytes()).collect::<Vec<_>>(),
        "coucou"
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
        Lexer::new("[a 1 b, {w, x, y, z}, 0.5]".bytes()).collect::<Vec<_>>(),
        "[a 1 b, {{w, x, y, z}}, 0.5]"
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
        Lexer::new(":hay: :hey:: not hay: :: :::: fin".bytes()).collect::<Vec<_>>(),
        ":hay: :hey:: not hay: :: :::: fin"
    );

    assert_eq!(
        &[
            Token(Location(0), Number(42.0)),
            Token(Location(3), Bytes(vec![42])),
            Token(Location(7), Number(42.0)),
            Token(Location(12), Number(42.0)),
            Token(Location(21), Number(42.0)),
        ][..],
        Lexer::new("42 :*: 0x2a 0b101010 0o52".bytes()).collect::<Vec<_>>(),
        "42 :*: 0x2a 0b101010 0o52"
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

    assert_tree!(
        "input",
        FrozenType::Bytes(false),
        Tree(Location(0), Apply(0, "input".into(), vec![]))
    );

    assert_tree!(
        "input, split:-:, map[tonum, add1, tostr], join:+:",
        FrozenType::Bytes(false),
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
        )
    );

    assert_tree!(
        "tonum, add234121, tostr, ln",
        FrozenType::Func(
            Box::new(FrozenType::Bytes(false)),
            Box::new(FrozenType::Bytes(true))
        ),
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
        )
    );

    assert_tree!(
        "[tonum, add234121, tostr] :13242:",
        FrozenType::Bytes(true),
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
        )
    );

    assert_tree!(
        "{repeat 1, {}}",
        FrozenType::List(
            true,
            Box::new(FrozenType::List(false, Box::new(FrozenType::Number)))
        ),
        Tree(
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
        )
    );

    assert_tree!(
        "add 1, tostr",
        FrozenType::Func(
            Box::new(FrozenType::Number),
            Box::new(FrozenType::Bytes(true))
        ),
        Tree(
            Location(5),
            Apply(
                0,
                COMPOSE_OP_FUNC_NAME.into(),
                vec![
                    Tree(
                        Location(0),
                        Apply(0, "add".into(), vec![Tree(Location(4), Number(1.0))])
                    ),
                    Tree(Location(7), Apply(0, "tostr".into(), vec![]))
                ]
            )
        )
    );

    assert_tree!(
        "zipwith add {1}",
        FrozenType::Func(
            Box::new(FrozenType::List(false, Box::new(FrozenType::Number))),
            Box::new(FrozenType::List(true, Box::new(FrozenType::Number)))
        ),
        Tree(
            Location(0),
            Apply(
                0,
                "zipwith".into(),
                vec![
                    Tree(Location(8), Apply(0, "add".into(), vec![])),
                    Tree(Location(12), List(0, vec![Tree(Location(13), Number(1.0))]))
                ]
            )
        )
    );
}
// }}}

// reporting {{{
#[test]
fn reporting() {
    use ErrorContext::*;
    use ErrorKind::*;
    use FrozenType::*;

    assert_maytree!(
        "{1, 2, 3, :soleil:}",
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(
                    Location(10),
                    ExpectedButGot {
                        expected: Number,
                        actual: Bytes(true)
                    }
                )),
                because: TypeListInferredItemType {
                    list_item_type: Number
                }
            }
        )]))
    );

    assert_maytree!(
        "{repeat 1, {}}",
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
        ))
    );

    assert_maytree!(
        "{{}, repeat 1}",
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(Location(5), InfWhereFinExpected)),
                because: TypeListInferredItemType {
                    list_item_type: List(true, Box::new(Named("itemof(typeof({}))".into())))
                }
            }
        )]))
    );

    assert_maytree!(
        "{{42}, repeat 1}",
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(Location(7), InfWhereFinExpected)),
                because: TypeListInferredItemType {
                    list_item_type: List(true, Box::new(Number))
                }
            }
        )]))
    );

    assert_maytree!(
        "{1, 2, 3, :soleil:}, ln",
        Err(ErrorList(vec![
            Error(
                Location(0),
                ContextCaused {
                    error: Box::new(Error(
                        Location(10),
                        ExpectedButGot {
                            expected: Number,
                            actual: Bytes(true)
                        }
                    )),
                    because: TypeListInferredItemType {
                        list_item_type: Number
                    }
                }
            ),
            Error(
                Location(21),
                ContextCaused {
                    error: Box::new(Error(
                        Location(0),
                        ExpectedButGot {
                            expected: Bytes(false),
                            actual: List(true, Box::new(Number))
                        }
                    )),
                    because: ChainedFromAsNthArgToNamedNowTyped {
                        comma_loc: Location(19),
                        nth_arg: 1,
                        func_name: "ln".into(),
                        type_with_curr_args: Func(Box::new(Bytes(false)), Box::new(Bytes(false)))
                    }
                }
            )
        ]))
    );

    assert_maytree!(
        "add 1 :2:",
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(
                    Location(6),
                    ExpectedButGot {
                        expected: Number,
                        actual: Bytes(true)
                    }
                )),
                because: AsNthArgToNamedNowTyped {
                    nth_arg: 2,
                    func_name: "add".into(),
                    type_with_curr_args: Func(Box::new(Number), Box::new(Number))
                }
            }
        )]))
    );

    assert_maytree!(
        ":42:, add 1",
        Err(ErrorList(vec![Error(
            Location(6),
            ContextCaused {
                error: Box::new(Error(
                    Location(0),
                    ExpectedButGot {
                        expected: Number,
                        actual: Bytes(true)
                    }
                )),
                because: ChainedFromAsNthArgToNamedNowTyped {
                    comma_loc: Location(4),
                    nth_arg: 2,
                    func_name: "add".into(),
                    type_with_curr_args: Func(Box::new(Number), Box::new(Number))
                }
            }
        )]))
    );

    assert_maytree!(
        "add 1, tostr",
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
        ))
    );

    assert_maytree!(
        "add 1, tonum",
        Err(ErrorList(vec![Error(
            Location(7),
            ContextCaused {
                error: Box::new(Error(
                    Location(0),
                    ContextCaused {
                        error: Box::new(Error(
                            Location(0),
                            ExpectedButGot {
                                expected: Bytes(false),
                                actual: Number
                            }
                        )),
                        because: CompleteType {
                            complete_type: Func(Box::new(Number), Box::new(Number))
                        }
                    }
                )),
                because: ChainedFromAsNthArgToNamedNowTyped {
                    comma_loc: Location(5),
                    nth_arg: 1,
                    func_name: "tonum".into(),
                    type_with_curr_args: Func(Box::new(Bytes(false)), Box::new(Number))
                }
            }
        )]))
    );

    assert_maytree!(
        "prout 1, caca",
        Err(ErrorList(vec![
            Error(
                Location(0),
                UnknownName {
                    name: "prout".into(),
                    expected_type: Func(
                        Box::new(Number),
                        Box::new(Named("paramof(typeof(caca))".into()))
                    )
                }
            ),
            Error(
                Location(9),
                UnknownName {
                    name: "caca".into(),
                    expected_type: Func(
                        Box::new(Named("paramof(typeof(caca))".into())),
                        Box::new(Named("returnof(typeof(caca))".into()))
                    )
                }
            )
        ]))
    );

    assert_maytree!(
        "_, split_, map_, join_",
        Err(ErrorList(vec![
            Error(
                Location(0),
                UnknownName {
                    name: "_".into(),
                    expected_type: Bytes(false)
                }
            ),
            Error(
                Location(8),
                UnknownName {
                    name: "_".into(),
                    expected_type: Bytes(true)
                }
            ),
            Error(
                Location(14),
                UnknownName {
                    name: "_".into(),
                    expected_type: Func(Box::new(Bytes(false)), Box::new(Bytes(false)))
                }
            ),
            Error(
                Location(21),
                UnknownName {
                    name: "_".into(),
                    expected_type: Bytes(true)
                }
            )
        ]))
    );

    assert_maytree!(
        "input, _",
        Err(ErrorList(vec![Error(
            Location(7),
            UnknownName {
                name: "_".into(),
                expected_type: Func(
                    Box::new(Bytes(false)),
                    Box::new(Named("returnof(typeof(_))".into()))
                )
            }
        )]))
    );

    assert_maytree!(
        "{const, _, add, map}",
        Err(ErrorList(vec![
            Error(
                Location(8),
                UnknownName {
                    name: "_".into(),
                    expected_type: Func(
                        Box::new(Number),
                        Box::new(Func(Box::new(Number), Box::new(Number)))
                    )
                }
            ),
            Error(
                Location(0),
                ContextCaused {
                    error: Box::new(Error(
                        Location(16),
                        ContextCaused {
                            error: Box::new(Error(
                                Location(16),
                                ExpectedButGot {
                                    expected: Func(
                                        Box::new(Named("a".into())),
                                        Box::new(Named("b".into()))
                                    ),
                                    actual: Number
                                }
                            )),
                            because: CompleteType {
                                complete_type: Func(
                                    Box::new(Func(
                                        Box::new(Named("a".into())),
                                        Box::new(Named("b".into()))
                                    )),
                                    Box::new(Func(
                                        Box::new(List(false, Box::new(Named("a".into())))),
                                        Box::new(List(false, Box::new(Named("b".into()))))
                                    ))
                                )
                            }
                        }
                    )),
                    because: TypeListInferredItemType {
                        list_item_type: Func(
                            Box::new(Number),
                            Box::new(Func(Box::new(Number), Box::new(Number)))
                        )
                    }
                }
            )
        ]))
    );

    assert_maytree!(
        "2 input",
        Err(ErrorList(vec![Error(
            Location(0),
            NotFunc {
                actual_type: Number
            }
        )]))
    );

    assert_maytree!(
        "input 2",
        Err(ErrorList(vec![Error(
            Location(0),
            TooManyArgs {
                nth_arg: 1,
                func_name: "input".into()
            }
        )]))
    );

    assert_maytree!(
        "add 1 2 3",
        Err(ErrorList(vec![Error(
            Location(0),
            TooManyArgs {
                nth_arg: 3,
                func_name: "add".into()
            }
        )]))
    );

    assert_maytree!(
        "3, add 1 2",
        Err(ErrorList(vec![Error(
            Location(0),
            ContextCaused {
                error: Box::new(Error(
                    Location(3),
                    TooManyArgs {
                        nth_arg: 3,
                        func_name: "add".into()
                    }
                )),
                because: ChainedFromToNotFunc {
                    comma_loc: Location(1)
                }
            }
        )]))
    );
}
// }}}
