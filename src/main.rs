use std::env;

fn main() {
    println!("Hello, world!");
    let script_args = env::args().skip(1);
    let script = script_args.collect::<Vec<String>>().join(" ");
    println!("-> _{script}_");
}
