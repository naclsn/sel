//! file-level scoping and resolving

use std::cell::RefCell;
use std::collections::HashMap;
use std::io::Result as IoResult;
use std::ops::Range;
use std::rc::Rc;

use crate::check::{Checker, Tree};
use crate::error::Error;
use crate::parse::{Parser, Top};

#[derive(PartialEq, Debug, Clone)]
pub struct Location(pub String, pub Range<usize>);

#[derive(Debug)]
pub struct Function {
    pub ast: Tree,
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
    // i dont like the use of 'load' with immut args, might rename to 'require' or idk but donnn
    // mater much anyways

    /// load `bytes` as a module's source and associate them with `path`
    ///
    /// `path` isn't altered/normalized in any way
    ///
    /// parsing in and by itself is not recursive, this does not hold on to a borrow
    /// during parsing (until insertion into the hash map)
    pub fn load_bytes(&self, path: &str, bytes: impl IntoIterator<Item = u8>) -> Rc<Module> {
        let bytes: Box<[u8]> = bytes.into_iter().collect();
        let line_map = Module::compute_line_map(&bytes);

        let mut parser = Parser::new(&path, bytes.iter().copied());
        let top = parser.parse_top();
        let errors = parser.boxed_errors();
        let module = Module {
            path: path.into(),
            bytes,
            line_map,
            top,
            errors,
            functions: Default::default(),
        };

        self.modules
            .borrow_mut()
            .entry(path.into())
            .insert_entry(module.into())
            .get()
            .clone()
    }

    /// try to load a module from `path` (ie fs as of now)
    ///
    /// `path` may be altered/normalized
    ///
    /// parsing in and by itself is not recursive, this does not hold on to a borrow
    /// even if it hadn't been parsed yet (until insertion into the hash map)
    pub fn load(&self, path: &str) -> IoResult<Rc<Module>> {
        let has = self.modules.borrow().get(path).cloned();
        if let Some(found) = has {
            return Ok(found);
        }

        Ok(self.load_bytes(path, std::fs::read(path)?))
    }
}

impl Module {
    /// the top level
    ///
    /// checking may be recursive if the function refers to 'def's
    /// within this module or a 'use'd module; registry's load may
    /// be invoked as needed
    pub fn retrieve(&self, registry: &ModuleRegistry) -> Option<Function> {
        let mut checker = Checker::new(self, registry);
        Some(Function {
            ast: checker.check_script(self.top.script.as_ref()?),
            errors: checker.boxed_errors(),
        })
    }

    /// search in 'use's
    ///
    /// functions are checked lazily, however this does not hold on to a borrow
    /// even if it hadn't been checked yet (until insertion into the hash map)
    pub fn retrieve_function(&self, name: &str, registry: &ModuleRegistry) -> Option<Rc<Function>> {
        let has = self.functions.borrow().get(name).cloned();
        if has.is_some() {
            return has;
        }

        let apply = &self.top.defs.iter().rev().find(|d| name == d.name)?.to;
        let mut checker = Checker::new(self, registry);
        let function = Function {
            ast: checker.check_apply(apply),
            errors: checker.boxed_errors(),
        };

        let r = self
            .functions
            .borrow_mut()
            .entry(name.to_string())
            .insert_entry(function.into())
            .get()
            .clone();
        Some(r)
    }

    /// search in 'def's
    ///
    /// if `prefix` is found among the module's 'def's,
    /// this uses the registry to try to loading
    pub fn retrieve_module(
        &self,
        prefix: &str,
        registry: &ModuleRegistry,
    ) -> Option<IoResult<Rc<Module>>> {
        let path = &self
            .top
            .uses
            .iter()
            .rev()
            .find(|u| prefix == u.prefix)?
            .path;
        Some(registry.load(path))
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
}
