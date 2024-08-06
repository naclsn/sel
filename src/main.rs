use std::env;

mod builtin;
mod interp;
mod parse;

use parse::{Error, Tree, TypeList};

fn main() {
    let mut types = TypeList::new();
    let (ty, tree) = match Tree::new_typed(
        env::args().skip(1).flat_map(|mut a| {
            a.push(' ');
            a.into_bytes()
        }),
        &mut types,
    ) {
        Ok(app) => app,
        Err(e) => {
            match e {
                Error::Unexpected(t) => eprintln!("Unexpected token {t:?}"),
                Error::UnexpectedEnd => eprintln!("Unexpected end of script"),
                Error::UnknownName(n) => eprintln!("Unknown name '{n}'"),
                Error::NotFunc(o, _) => {
                    eprintln!("Expected a function type, but got {}", types.repr(o))
                }
                Error::ExpectedButGot(w, g, _) => {
                    eprintln!("Expected type {}, but got {}", types.repr(w), types.repr(g))
                }
                Error::InfWhereFinExpected => {
                    eprintln!("Expected finite type, but got infinite type")
                }
            }
            return;
        }
    };

    println!("{} ## {tree}", types.repr(ty));

    let val = interp::interp(&tree);
    interp::run_print(val);

    //Ok(())
}
