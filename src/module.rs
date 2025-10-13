//! file-level scoping and resolving

use std::cell::RefCell;
use std::collections::HashMap;
use std::fs::File;
use std::io::{Read, Result as IoResult};
use std::ops::{Deref, DerefMut, Range};
use std::path::{Path, PathBuf};
use std::rc::Rc;

use crate::check::{Checker, Tree};
use crate::error::Error;
use crate::fund::Fund;
use crate::parse::{Def, Parser, Script, Top, Use};
use crate::types::TypeRef;

// scope {{{
#[derive(Debug, Default)]
pub struct Scoping {
    bindings: Vec<HashMap<String, Binding>>,
    defineds: HashMap<String, Defined>,
    missings: HashMap<String, TypeRef>,
}

#[derive(Debug)]
pub enum Entry<'scope> {
    Binding(&'scope Binding),
    Defined(&'scope Defined),
    Fundamental(Fund),
}

#[derive(Debug)]
pub struct Binding {
    pub loc: Location,
    pub word: String,
    pub ty: TypeRef,
}

#[derive(Debug)]
pub struct Defined {
    pub loc: Location,
    pub desc: String,
    pub ty: TypeRef,
}

impl Scoping {
    pub fn push(&mut self, entries: HashMap<String, (Location, TypeRef)>) {
        self.bindings.push(
            entries
                .into_iter()
                .map(|(word, (loc, ty))| (word.clone(), Binding { loc, word, ty }))
                .collect(),
        );
    }

    pub fn pop(&mut self) {
        self.bindings
            .pop()
            .expect("unbalanced scoping pop with no push");
    }

    pub fn define(&mut self, name: String, loc: Location, desc: String, ty: TypeRef) -> &Defined {
        self.defineds
            .insert(name.clone(), Defined { loc, desc, ty });
        self.defineds.get(&name).unwrap()
    }

    pub fn missing(&mut self, name: String, ty: TypeRef) {
        assert!(
            self.missings.insert(name, ty).is_none(),
            "was not actually missing",
        );
    }

    /// lookup order is always:
    /// - fundamentals
    /// - bindings (closest to furthest)
    /// - defineds (latest to earliest in file)
    /// - uses (same order)
    /// - missing
    ///
    /// when None, likely want to call `missing()`
    /// when ToBeDefined, likely want to call `define()`
    pub fn lookup(&mut self, name: &str) -> Option<Entry> {
        if let Some(found) = Fund::try_from_name(name) {
            return Some(Entry::Fundamental(found));
        }
        if let Some(found) = self.bindings.iter().rev().find_map(|it| it.get(name)) {
            return Some(Entry::Binding(found));
        }
        if let Some(found) = self.defineds.get(name) {
            return Some(Entry::Defined(found));
        }
        None
    }
}
// }}}

// source registry {{{
#[derive(PartialEq, Debug, Clone)]
pub struct Location(pub String, pub Range<usize>);

#[derive(Debug)]
struct Function {
    ast: Tree,
    pub errors: Box<[Error]>,
}

#[derive(Debug)]
pub struct Module {
    pub path: String,
    pub bytes: Box<[u8]>,
    line_map: Box<[Range<usize>]>,
    top: Top,
    pub errors: Box<[Error]>,
    functions: RefCell<HashMap<String, Rc<Function>>>,
}

#[derive(Default)]
pub struct ModuleRegistry {
    modules: RefCell<HashMap<String, Rc<Module>>>,
}

impl ModuleRegistry {
    pub fn load_bytes(&mut self, path: &str, bytes: impl IntoIterator<Item = u8>) -> Rc<Module> {
        let bytes: Box<[u8]> = bytes.into_iter().collect();
        let mut parser = Parser::new(path, bytes.iter().copied());
        let path = path.to_string();
        let module = Module {
            path,
            bytes,
            line_map: Module::compute_line_map(&bytes),
            top: parser.parse_top(),
            errors: parser.boxed_errors(),
            functions: Default::default(),
        };
        self.modules
            .borrow_mut()
            .entry(path.clone())
            .insert_entry(module.into())
            .get()
            .clone()
    }

    pub fn load(&mut self, path: &str) -> IoResult<Rc<Module>> {
        let borrow = self.modules.borrow();
        Ok(match borrow.get(path).cloned() {
            Some(module) => module,
            None => {
                drop(borrow); // idk y but this necessary here
                self.load_bytes(path, std::fs::read(path)?)
            }
        })
    }
}

impl Module {
    /// the top level
    pub fn retrieve(&self, registry: &ModuleRegistry) -> Option<Function> {
        let mut checker = Checker::new(self, registry);
        Some(Function {
            ast: checker.check_script(self.top.script.as_ref()?),
            errors: checker.boxed_errors(),
        })
    }

    /// search in 'use's
    pub fn retrieve_function(&self, name: &str, registry: &ModuleRegistry) -> Option<Rc<Function>> {
        Some(match self.functions.borrow().get(name).cloned() {
            Some(function) => function,
            None => {
                let apply = &self.top.defs.iter().find(|d| name == d.name)?.to;
                let mut checker = Checker::new(self, registry);
                let function = Function {
                    ast: checker.check_apply(apply),
                    errors: checker.boxed_errors(),
                };
                self.functions
                    .borrow_mut()
                    .entry(name.to_string())
                    .insert_entry(function.into())
                    .get()
                    .clone()
            }
        })
    }

    fn compute_line_map(bytes: &[u8]) -> Box<[Range<usize>]> {
        let mut r = Vec::new();

        let mut it = bytes.iter();
        let mut at = 0;
        while let Some(nl) = it.position(|c| b'\n' == *c) {
            r.push(at..at + nl);
            at += nl + 1;
        }
        if at < bytes.len() {
            r.push(at..bytes.len());
        }

        r.into_boxed_slice()
    }

    pub fn get_containing_lnum(&self, k: usize) -> Option<usize> {
        self.line_map
            .iter()
            .position(|r| r.contains(&k))
            .map(|l| l + 1)
    }

    /// the tuple's first item is the same as if `source.get_containing_lnum(k.start)`
    pub fn get_containing_lines(&self, k: &Range<usize>) -> Option<(usize, &[Range<usize>])> {
        let mut it = self.line_map.iter();
        let start = it.position(|r| k.start <= r.end)?;
        let eof = self.line_map.len() - start;
        let end = start + it.position(|r| k.end < r.start).unwrap_or(eof - 1);
        Some((start + 1, &self.line_map[start..=end]))
    }

    /// same but does not check parents (local uses)
    fn get_if_has_locally(&mut self, local_name: &str) -> Option<()> {
        if let Some(found) = self.top.defs.iter().rev().find(|it| local_name == it.name) {
            return Some(todo!("{found:?}"));
        }

        None
    }

    pub fn get_if_has(&mut self, local_name: &str) -> Option<()> {
        let locally = self.get_if_has_locally(local_name);
        if locally.is_some() {
            return locally;
        }

        for parent in self.top.uses.iter().rev() {
            if let Some(parent_local_name) = local_name
                .strip_prefix(&parent.prefix)
                .and_then(|n| n.strip_prefix('_'))
            {
                let reg: &mut ModuleRegistry = self.registry.borrow_mut();
                let source = reg
                    .load(String::from_utf8(parent.path.to_vec()).expect("lskdjf"))
                    .expect("report it only once");
                let parent_locals = reg.get(source);
                return parent_locals.get_if_has_locally(parent_local_name);
            }
        }

        None
    }
}
// }}}
