mod check;
mod errors;
mod format;
mod fund;
mod lex;
mod module;
mod parse;
mod types;

fn main() {
    let mut registry = module::ModuleRegistry::default();
    let text = std::env::args()
        .skip(1)
        .flat_map(|a| (a + " ").into_bytes());

    let module = registry.load_bytes("<args>", text);
    if !module.errors.is_empty() {
        crate::errors::report_many_stderr(&module.errors, &registry, &None, false);
    }
    eprintln!("CST: {module:#?}");

    let function = module.retrieve(&mut registry).unwrap();
    if !function.errors.is_empty() {
        crate::errors::report_many_stderr(&function.errors, &registry, &None, false);
    }
    eprintln!("AST: {function:#?}");

    eprintln!("ty: {}", function.ast.ty);
}
