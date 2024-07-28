use std::io::{self, Write};

mod parse;

fn main() {
    print!(">: ");
    let _ = io::stdout().flush();
    let expr = parse::Tree::from_stream(io::stdin()).unwrap();
    println!("{expr:?}");
}
