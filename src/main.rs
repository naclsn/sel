//use std::fmt::Debug;
use std::io::{self, Write};

mod builtin;
mod parse;
mod typing;

use parse::{Error, Tree};
use typing::Types;

fn main() -> Result<(), Error> {
    print!(">: ");
    let _ = io::stdout().flush();

    let mut types = Types::new();
    let tree = Tree::from_stream(io::stdin(), &mut types)?;

    let ty = tree.make_type(&mut types)?;
    types.get(ty).repr(&types, &mut io::stdout()).unwrap();

    println!(" ## {tree}");

    //    let expr = match App::from_tree(tree) {
    //        Ok(ok) => ok,
    //        Err(err) => return Err(Box::new(err)),
    //    };
    //    println!("{expr:#?}");
    //
    //    match expr.types[expr.atree.ty].repr(&expr.types, &mut io::stdout()) {
    //        Ok(()) => Ok(()),
    //        Err(err) => return Err(Box::new(err)),
    //    }
    Ok(())
}
