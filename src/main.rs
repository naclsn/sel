use std::env;
use std::io::{self, Write};

mod builtin;
mod interp;
mod parse;
mod typing;

use interp::Value;
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

    let val = interp::interp(&tree);
    run_print(val);

    Ok(())
}

fn run_print(val: Value) {
    match val {
        Value::Number(n) => println!("{}", n.0()),
        Value::Bytes(mut b) => {
            while let Some(ch) = b.0() {
                io::stdout().write_all(&ch).unwrap();
            }
        }
        Value::List(l) => {
            for it in l {
                run_print(it);
                println!();
            }
        }
        Value::Func(_f) => todo!(),
    }
}
