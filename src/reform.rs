use crate::parse::{Token, Tree};

#[derive(PartialEq, Debug)]
pub enum Value {
    Number(i32),
    Text(Vec<u8>),
    List(Vec<ATree>),
    Func(String),
}

#[derive(PartialEq, Debug)]
pub struct ATree(Value, Vec<ATree>);

#[derive(PartialEq, Debug)]
pub enum Error {
    NotFunc(Value),
}

impl ATree {
    pub fn from_tree(tree: Tree) -> Result<ATree, Error> {
        // A, B, C => C [B A]
        // [A B] C => A B C
        // [A, B] C => B A C

        Ok(match tree {
            Tree::Apply(base, args) => {
                // [A, B] C => B A C
                let ATree(base2, mut args2) = ATree::from_tree(*base)?;
                let Value::Func(_) = base2 else {
                    return Err(Error::NotFunc(base2));
                };
                // [A B] C => A B C
                args2.reserve(args.len());
                for it in args.into_iter().map(ATree::from_tree) {
                    args2.push(it?);
                }
                ATree(base2, args2)
            }

            Tree::Atom(Token::Word(name)) => ATree(Value::Func(name), vec![]),
            Tree::Atom(Token::Number(n)) => ATree(Value::Number(n), vec![]),
            Tree::Atom(Token::Text(t)) => ATree(Value::Text(t), vec![]),
            Tree::Atom(_) => unreachable!(),

            // A, B, C => C [B A]
            Tree::Chain(v) => v
                .into_iter()
                .map(ATree::from_tree)
                .reduce(|acc, cur| {
                    let (acc, mut cur) = (acc?, cur?);
                    cur.1.push(acc);
                    Ok(cur)
                })
                .unwrap()?,

            Tree::List(l) => ATree(
                Value::List(
                    l.into_iter()
                        .map(ATree::from_tree)
                        .collect::<Result<_, _>>()?,
                ),
                vec![],
            ),
        })
    }
}
