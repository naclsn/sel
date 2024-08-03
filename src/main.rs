use std::env;

mod builtin;
mod interp;
mod parse;

use parse::{Error, Tree, TypeList};

fn main() -> Result<(), Error> {
    let mut types = TypeList::new();
    let (ty, tree) = Tree::new_typed(
        env::args().skip(1).flat_map(|mut a| {
            a.push(' ');
            a.into_bytes()
        }),
        &mut types,
    )?;

    println!("{} ## {tree}", types.repr(ty));

    let val = interp::interp(&tree);
    interp::run_print(val);

    Ok(())
}
