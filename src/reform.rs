use crate::parse::{Token, Tree};
use crate::builtin::lookup;
use crate::value::{Boundedness, Type, Value};

#[derive(PartialEq, Debug)]
pub struct ATree {
    base: Value,
    args: Vec<ATree>,
    pub ty: usize,
}

#[derive(PartialEq, Debug)]
pub struct App {
    pub atree: ATree,
    pub types: Vec<Type>,
}

#[derive(PartialEq, Debug)]
pub enum Error {
    NotFunc(usize, Vec<Type>),
    ExpectedButGot(usize, usize, Vec<Type>),
    UnknowName(String),
}

impl App {
    fn atree_from_tree(tree: Tree, types: &mut Vec<Type>) -> Result<ATree, Error> {
        // A, B, C => C [B A]
        // [A B] C => A B C
        // [A, B] C => B A C

        Ok(match tree {
            Tree::Apply(base, args) => {
                // [A, B] C => [B A] C
                let mut r = App::atree_from_tree(*base, types)?;
                // [A B] C => A B C
                r.args.reserve(args.len());
                for a in args {
                    let it = App::atree_from_tree(a, types)?;
                    r.ty = Type::applied(r.ty, it.ty, types)?;
                    r.args.push(it);
                }
                r
            }

            Tree::Atom(Token::Word(name)) => match lookup(&name, types) {
                Ok(()) => ATree {
                    base: Value::Func(name),
                    args: vec![],
                    ty: types.len()-1,
                },
                Err(()) => return Err(Error::UnknowName(name.to_string())),
            },
            Tree::Atom(Token::Number(n)) => ATree {
                base: Value::Number(n),
                args: vec![],
                ty: 0, //Type::Number,
            },
            Tree::Atom(Token::Bytes(t)) => ATree {
                base: Value::Bytes(t),
                args: vec![],
                ty: 1, //Type::Bytes(Boundedness::Finite),
            },
            Tree::Atom(_) => unreachable!(),

            Tree::Chain(v) => {
                // A, B, C => C [B A]
                let mut iter = v.into_iter();
                let mut acc = App::atree_from_tree(iter.next().unwrap(), types)?;
                for a in iter {
                    let mut cur = App::atree_from_tree(a, types)?;
                    cur.ty = Type::applied(cur.ty, acc.ty, types)?;
                    cur.args.push(acc);
                    acc = cur;
                }
                acc
            },

            Tree::List(l) => {
                // ini -> {num, str, lst[..], fun[..]}
                // num -> str
                // any -> same
                ATree {
                    base: Value::List(
                        l.into_iter()
                            .map(|a| App::atree_from_tree(a, types))
                            .collect::<Result<_, _>>()?,
                    ),
                    args: vec![],
                    ty: 0, // XXX
                }
            },
        })
    }

    pub fn from_tree(tree: Tree) -> Result<App, Error> {
        let mut types = vec![Type::Number, Type::Bytes(Boundedness::Finite)];
        Ok(App {
            atree: App::atree_from_tree(tree, &mut types)?,
            types,
        })
    }
}
