use std::fmt::Debug;
use std::io::{self, Write};

mod parse;
mod reform;

use parse::{Token, Tree};
use reform::ATree;

fn main() -> Result<(), Box<dyn Debug>> {
    print!(">: ");
    let _ = io::stdout().flush();

    let input = Tree::Atom(Token::Word("input".to_string()));
    let tree = match Tree::from_stream(io::stdin()) {
        Ok(Tree::Chain(mut v)) => {
            v.insert(0, input);
            Tree::Chain(v)
        }
        Ok(other) => Tree::Chain(vec![other, input]),
        Err(err) => return Err(Box::new(err)),
    };

    let expr = match ATree::from_tree(tree) {
        Ok(ok) => ok,
        Err(err) => return Err(Box::new(err)),
    };

    println!("{expr:#?}");
    Ok(())
}
