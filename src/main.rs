use std::io;

mod parse;

fn main() {
    print!(">: ");
    let expr = parse::Tree::from_stream(io::stdin()).unwrap();
    println!("{expr:?}");
}
