use std::cell::RefCell;
use std::collections::HashMap;
use std::path::PathBuf;
use std::rc::Rc;

use crate::{check::*, parse::*, scope::*, types::*};

struct Locals {
    registry: ModuleRegistry,

    top: Top,
    /// hash from local (unprefixed) name to thingy
    funcs: HashMap<String, Tree>, // Func directly, and would use the SourceRef+local_name

    /// actually lets just move the TypeList and FuncList inside the Resolver
    /// and make them source file -local resources;
    /// when a function is called, its type needs to be 'duplicate()'-ed (instantiated)
    /// so its type vars can be assigned, so best is to use the missing 'duplicate_into()'
    types: TypeList,
}
///// and the func ref thing would be used in a FuncList that's global like TypeList
//type FuncRef = usize;

impl Locals {
    pub fn new(registry: ModuleRegistry, source: ModuleRef) -> Self {
        let reg = registry.borrow();
        let bytes = &reg.get(source).bytes;

        let mut parser = Parser::new(source, bytes.iter().copied());
        let top = parser.parse_top();
        drop(reg);

        Self {
            registry,
            top,
            funcs: HashMap::default(),
            types: TypeList::default(),
        }
    }

    /// same but does not check parents (local uses)
    fn get_if_has_locally(&mut self, local_name: &str) -> Option<()> {
        if let Some(found) = self.top.defs.iter().rev().find(|it| local_name == it.name) {
            return Some(todo!("{found:?}"));
        }

        None
    }

    /// name would already be prefixed..? na, it's removed before calling
    pub fn get_if_has(&mut self, local_name: &str) -> Option<()> {
        let locally = self.get_if_has_locally(local_name);
        if locally.is_some() {
            return locally;
        }

        //else // check parents (local uses)
        for parent in self.top.uses.iter().rev() {
            if let Some(parent_local_name) = local_name
                .strip_prefix(&parent.prefix)
                .and_then(|n| n.strip_prefix('_'))
            {
                let reg = self.registry.borrow_mut();
                let source = reg
                    .add(String::from_utf8(parent.path.to_vec()).expect("lskdjf"))
                    .expect("report it only once");
                // ^ the add thingy is weird anyways
                let parent_locals: &mut Locals = reg.get(source).locals;
                return parent_locals.get_if_has_locally(parent_local_name);
            }
        }

        None
    }
}
