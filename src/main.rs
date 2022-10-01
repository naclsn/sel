use crate::engine::*;

// mod parser;
mod engine;
// mod prelude;

fn main() {
    println!();

    // id :: a -> a
    let id = Value::Fun(Box::new(Function {
        name: "id".to_string(),
        arity: 1usize,
        args: vec![],
        func: |args| args[0].clone(),
    }));

    {
        let arg = Value::Str("coucou".to_string());
        println!("{id:?} {arg:?}");
        let res = id.apply(arg);
        println!(" = {res:?}");
    }

    println!();

    // add :: a -> (a -> a)
    let add = Value::Fun(Box::new(Function {
        name: "add".to_string(),
        arity: 2usize,
        args: vec![],
        func: |args| {
            match (&args[0], &args[1]) {
                (Value::Num(a), Value::Num(b)) => Value::Num(a+b),
                _ => panic!("type error"),
            }
        },
    }));

    {
        let arg1 = Value::Num(1.0);
        let arg2 = Value::Num(2.0);
        println!("{add:?} {arg1:?} {arg2:?}");
        let res1 = add.apply(arg1);
        println!(" = {res1:?} {arg2:?}");
        let res2 = res1.apply(arg2);
        println!(" = {res2:?}");
    }

    println!();

    // reverse :: [a] -> [a]
    let reverse = Value::Fun(Box::new(Function {
        name: "reverse".to_string(),
        arity: 1usize,
        args: vec![],
        func: |args| {
            match &args[0] {
                Value::Arr(v) => Value::Arr(v
                    .iter()
                    .map(|it| it.clone())
                    .rev()
                    .collect()
                ),
                _ => panic!("type error"),
            }
        },
    }));

    {
        let arg = Value::Arr(vec![Value::Num(1.0), Value::Num(2.0), Value::Num(3.0)]);
        println!("{reverse:?} {arg:?}");
        let res = reverse.apply(arg);
        println!(" = {res:?}");
    }

}
