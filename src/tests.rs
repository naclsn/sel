use crate::error::{Error, ErrorContext, ErrorKind, ErrorList};
use crate::parse::{Applicable, Lexer, Location, Token, TokenKind, Tree, TreeKind};
use crate::types::FrozenType;

// utils {{{
fn expect<T>(r: Result<T, ErrorList>, script: &str) -> T {
    r.unwrap_or_else(|err| {
        let mut n = 0;
        for e in err {
            eprintln!("!!! {e:?}");
            n += 1;
        }
        eprintln!("({n} error{})", if 1 == n { "" } else { "s" });
        panic!("{}", script)
    })
}

macro_rules! emptree {
    () => {
        Tree {
            loc: Location(0),
            ty: 0,
            value: TreeKind::Number(0.0),
        }
    };
}

macro_rules! assert_toks {
    ($script:literal, $($stream:expr,)*) => {
        assert_eq!(
            &[$($stream),*][..] as &[Token],
            Lexer::new($script.bytes())
                .take_while(|t| TokenKind::End != t.1)
                .collect::<Vec<_>>(),
            "{}", $script
        );
    };
}

macro_rules! assert_tree {
    ($script:literal, $type:expr, $tree:expr) => {
        let (ty, res) = expect(Tree::new_typed($script.bytes()), $script);
        assert_eq!(&$tree, &res, "{}", $script);
        assert_eq!($type, ty, "{}", $script);
    };
}

macro_rules! assert_maytree {
    ($script:literal, $result:expr) => {
        assert_eq!(&$result, &Tree::new($script.bytes()), "{}", $script);
    };
}
// }}}

// lexing {{{
#[test]
fn lexing() {
    use TokenKind::*;

    assert_toks!("",);

    assert_toks!("coucou", Token(Location(0), Word("coucou".into())),);

    assert_toks!(
        "[a 1 b, {w, x, y, z}, 0.5]",
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
        Token(Location(25), CloseBracket),
    );

    assert_toks!(
        ":hay: :hey:: not hay: :: :::: fin",
        Token(Location(0), Bytes(vec![104, 97, 121])),
        Token(
            Location(6),
            Bytes(vec![104, 101, 121, 58, 32, 110, 111, 116, 32, 104, 97, 121])
        ),
        Token(Location(22), Bytes(vec![])),
        Token(Location(25), Bytes(vec![58])),
        Token(Location(30), Word("fin".into())),
    );

    assert_toks!(
        "42 :*: 0x2a 0b101010 0o52",
        Token(Location(0), Number(42.0)),
        Token(Location(3), Bytes(vec![42])),
        Token(Location(7), Number(42.0)),
        Token(Location(12), Number(42.0)),
        Token(Location(21), Number(42.0)),
    );

    assert_toks!(
        "don't let it be the def of cat!",
        Token(Location(0), Word("don".into())),
        Token(Location(3), Unknown("'".into())),
        Token(Location(4), Word("t".into())),
        Token(Location(6), TokenKind::Let),
        Token(Location(10), Word("it".into())),
        Token(Location(13), Word("be".into())),
        Token(Location(16), Word("the".into())),
        Token(Location(20), TokenKind::Def),
        Token(Location(24), Word("of".into())),
        Token(Location(27), Word("cat".into())),
        Token(Location(30), Unknown("!".into())),
    );
}
// }}}

// parsing {{{
#[test]
fn parsing() {
    use Applicable::*;
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
        Tree {
            loc: Location(0),
            ty: 4,
            value: Apply(Name("input".into()), vec![])
        }
    );

    assert_tree!(
        "input, split:-:, map[tonum, add1, tostr], join:+:",
        FrozenType::Bytes(false),
        Tree {
            loc: Location(42),
            ty: 46,
            value: Apply(
                Name("join".into()),
                vec![
                    Tree {
                        loc: Location(46),
                        ty: 1,
                        value: Bytes(vec![43])
                    },
                    Tree {
                        loc: Location(17),
                        ty: 16,
                        value: Apply(
                            Name("map".into()),
                            vec![
                                Tree {
                                    loc: Location(20),
                                    ty: 38,
                                    value: Apply(
                                        Name("compose".into()),
                                        vec![
                                            Tree {
                                                loc: Location(26),
                                                ty: 29,
                                                value: Apply(
                                                    Name("compose".into()),
                                                    vec![
                                                        Tree {
                                                            loc: Location(21),
                                                            ty: 21,
                                                            value: Apply(
                                                                Name("tonum".into()),
                                                                vec![]
                                                            )
                                                        },
                                                        Tree {
                                                            loc: Location(28),
                                                            ty: 22,
                                                            value: Apply(
                                                                Name("add".into()),
                                                                vec![Tree {
                                                                    loc: Location(31),
                                                                    ty: 0,
                                                                    value: Number(1.0)
                                                                }]
                                                            )
                                                        }
                                                    ]
                                                )
                                            },
                                            Tree {
                                                loc: Location(34),
                                                ty: 32,
                                                value: Apply(Name("tostr".into()), vec![])
                                            }
                                        ]
                                    )
                                },
                                Tree {
                                    loc: Location(7),
                                    ty: 8,
                                    value: Apply(
                                        Name("split".into()),
                                        vec![
                                            Tree {
                                                loc: Location(12),
                                                ty: 1,
                                                value: Bytes(vec![45])
                                            },
                                            Tree {
                                                loc: Location(0),
                                                ty: 4,
                                                value: Apply(Name("input".into()), vec![])
                                            }
                                        ]
                                    )
                                }
                            ]
                        )
                    }
                ]
            )
        }
    );

    assert_tree!(
        "tonum, add234121, tostr, ln",
        FrozenType::Func(
            Box::new(FrozenType::Bytes(false)),
            Box::new(FrozenType::Bytes(true))
        ),
        Tree {
            loc: Location(23),
            ty: 34,
            value: Apply(
                Name("compose".into()),
                vec![
                    Tree {
                        loc: Location(16),
                        ty: 22,
                        value: Apply(
                            Name("compose".into()),
                            vec![
                                Tree {
                                    loc: Location(5),
                                    ty: 13,
                                    value: Apply(
                                        Name("compose".into()),
                                        vec![
                                            Tree {
                                                loc: Location(0),
                                                ty: 5,
                                                value: Apply(Name("tonum".into()), vec![])
                                            },
                                            Tree {
                                                loc: Location(7),
                                                ty: 6,
                                                value: Apply(
                                                    Name("add".into()),
                                                    vec![Tree {
                                                        loc: Location(10),
                                                        ty: 0,
                                                        value: Number(234121.0)
                                                    }]
                                                )
                                            }
                                        ]
                                    )
                                },
                                Tree {
                                    loc: Location(18),
                                    ty: 16,
                                    value: Apply(Name("tostr".into()), vec![])
                                }
                            ]
                        )
                    },
                    Tree {
                        loc: Location(25),
                        ty: 28,
                        value: Apply(Name("ln".into()), vec![])
                    }
                ]
            )
        }
    );

    assert_tree!(
        "[tonum, add234121, tostr] :13242:",
        FrozenType::Bytes(true),
        Tree {
            loc: Location(19),
            ty: 1,
            value: Apply(
                Name("tostr".into()),
                vec![Tree {
                    loc: Location(8),
                    ty: 0,
                    value: Apply(
                        Name("add".into()),
                        vec![
                            Tree {
                                loc: Location(11),
                                ty: 0,
                                value: Number(234121.0)
                            },
                            Tree {
                                loc: Location(1),
                                ty: 0,
                                value: Apply(
                                    Name("tonum".into()),
                                    vec![Tree {
                                        loc: Location(26),
                                        ty: 1,
                                        value: Bytes(vec![49, 51, 50, 52, 50])
                                    }]
                                )
                            }
                        ]
                    )
                }]
            )
        }
    );

    assert_tree!(
        "{repeat 1, {}}",
        FrozenType::List(
            true,
            Box::new(FrozenType::List(false, Box::new(FrozenType::Number)))
        ),
        Tree {
            loc: Location(0),
            ty: 10,
            value: List(vec![
                Tree {
                    loc: Location(1),
                    ty: 6,
                    value: Apply(
                        Name("repeat".into()),
                        vec![Tree {
                            loc: Location(8),
                            ty: 0,
                            value: Number(1.0)
                        }]
                    )
                },
                Tree {
                    loc: Location(11),
                    ty: 9,
                    value: List(vec![])
                }
            ])
        }
    );

    assert_tree!(
        "add 1, tostr",
        FrozenType::Func(
            Box::new(FrozenType::Number),
            Box::new(FrozenType::Bytes(true))
        ),
        Tree {
            loc: Location(5),
            ty: 11,
            value: Apply(
                Name("compose".into()),
                vec![
                    Tree {
                        loc: Location(0),
                        ty: 3,
                        value: Apply(
                            Name("add".into()),
                            vec![Tree {
                                loc: Location(4),
                                ty: 0,
                                value: Number(1.0)
                            }]
                        )
                    },
                    Tree {
                        loc: Location(7),
                        ty: 5,
                        value: Apply(Name("tostr".into()), vec![])
                    }
                ]
            )
        }
    );

    assert_tree!(
        "zipwith add {1}",
        FrozenType::Func(
            Box::new(FrozenType::List(false, Box::new(FrozenType::Number))),
            Box::new(FrozenType::List(true, Box::new(FrozenType::Number)))
        ),
        Tree {
            loc: Location(0),
            ty: 14,
            value: Apply(
                Name("zipwith".into()),
                vec![
                    Tree {
                        loc: Location(8),
                        ty: 18,
                        value: Apply(Name("add".into()), vec![])
                    },
                    Tree {
                        loc: Location(12),
                        ty: 20,
                        value: List(vec![Tree {
                            loc: Location(13),
                            ty: 0,
                            value: Number(1.0)
                        }])
                    }
                ]
            )
        }
    );
}
// }}}

// FIXME: with auto insert of coercion function,
// many of these make no sense because they
// won't trigger the report they were testing
// reporting {{{
//#[test]
fn reporting() {
    use Applicable::*;
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
        Ok(Tree {
            loc: Location(0),
            ty: 10,
            value: TreeKind::List(vec![
                Tree {
                    loc: Location(1),
                    ty: 6,
                    value: TreeKind::Apply(
                        Name("repeat".into()),
                        vec![Tree {
                            loc: Location(8),
                            ty: 0,
                            value: TreeKind::Number(1.0)
                        }]
                    )
                },
                Tree {
                    loc: Location(11),
                    ty: 9,
                    value: TreeKind::List(vec![])
                }
            ])
        })
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
                    because: ChainedFromAsNthArgToNowTyped {
                        comma_loc: Location(19),
                        nth_arg: 1,
                        func: Applicable::Name("ln".into()),
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
                because: AsNthArgToNowTyped {
                    nth_arg: 2,
                    func: Applicable::Name("add".into()),
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
                because: ChainedFromAsNthArgToNowTyped {
                    comma_loc: Location(4),
                    nth_arg: 2,
                    func: Applicable::Name("add".into()),
                    type_with_curr_args: Func(Box::new(Number), Box::new(Number))
                }
            }
        )]))
    );

    assert_maytree!("add 1, tostr", Ok(emptree!()));

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
                because: ChainedFromAsNthArgToNowTyped {
                    comma_loc: Location(5),
                    nth_arg: 1,
                    func: Applicable::Name("tonum".into()),
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
                func: Applicable::Name("input".into())
            }
        )]))
    );

    assert_maytree!(
        "add 1 2 3",
        Err(ErrorList(vec![Error(
            Location(0),
            TooManyArgs {
                nth_arg: 3,
                func: Applicable::Name("add".into())
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
                        func: Applicable::Name("add".into())
                    }
                )),
                because: ChainedFromToNotFunc {
                    comma_loc: Location(1)
                }
            }
        )]))
    );

    assert_maytree!(
        ":1:, tostr, _",
        Err(ErrorList(vec![
            Error(
                Location(5),
                ContextCaused {
                    error: Box::new(Error(
                        Location(0),
                        ExpectedButGot {
                            expected: Number,
                            actual: Bytes(true)
                        }
                    )),
                    because: ChainedFromAsNthArgToNowTyped {
                        comma_loc: Location(3),
                        nth_arg: 1,
                        func: Applicable::Name("tostr".into()),
                        type_with_curr_args: Func(Box::new(Number), Box::new(Bytes(true)))
                    }
                }
            ),
            Error(
                Location(12),
                UnknownName {
                    name: "_".into(),
                    expected_type: Func(
                        Box::new(Bytes(true)),
                        Box::new(Named("returnof(typeof(_))".into()))
                    )
                }
            )
        ]))
    );
}
// }}}
