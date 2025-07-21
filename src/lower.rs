use crate::parse::Pattern;
use crate::scope::SourceRef;
use crate::types::TypeRef;

#[derive(Debug, Clone)]
pub struct Tree {
    ty: TypeRef,
    val: TreeVal,
}

#[derive(Debug, Clone)]
pub enum TreeVal {
    Word(String, Refers),
    Bytes(Vec<u8>),
    Number(f64),
    List(Vec<Tree>),
    Pair(Box<Tree>, Box<Tree>),
    Apply(Box<Tree>, Vec<Tree>), // base can only be 'Word' or 'Binding'
    Binding(Pattern, Box<Tree>, Box<Tree>), // garbage if `pat.is_irrefutable()`
}

#[derive(Debug, Clone)]
pub enum Refers {
    Binding(()),     // from an enclosing let binding
    File(SourceRef), // from external (user) file
    Builtin(String), // eg from prelude
    Fundamental,     // eg cons, panic, bytes...
}
