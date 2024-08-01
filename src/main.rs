use std::env;
use std::io;

mod builtin;
mod parse;
mod typing;

use parse::{Error, Tree};
use typing::Types;

fn main() -> Result<(), Error> {
    let mut types = Types::new();
    let tree = Tree::new(
        env::args().skip(1).flat_map(|mut a| {
            a.push(' ');
            a.into_bytes()
        }),
        &mut types,
    )?;

    let ty = tree.make_type(&mut types)?;
    types.get(ty).repr(&types, &mut io::stdout()).unwrap();
    println!(" ## {tree}");

    Ok(())
}
