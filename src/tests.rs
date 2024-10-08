use insta::{assert_debug_snapshot, assert_snapshot};

use crate::error::ErrorList;
use crate::interp;
use crate::parse::{self, Lexer, TokenKind, Tree};
use crate::scope::Global;
use crate::types::FrozenType;

#[test]
fn lexing() {
    fn t(script: &[u8]) -> String {
        let mut end = true;
        Lexer::new(0, script.iter().copied())
            .take_while(|t| {
                if !end {
                    false
                } else {
                    end = TokenKind::End != t.1;
                    true
                }
            })
            .fold(String::new(), |mut acc, cur| {
                acc += &format!("{cur:?}\n");
                acc
            })
    }

    assert_snapshot!(t(b""));
    assert_snapshot!(t(b"coucou"));
    assert_snapshot!(t(b"[a 1 b, {w, x, y, z}, 0.5]"));
    assert_snapshot!(t(b":hay: :hey:: not hay: :: :::: fin"));
    assert_snapshot!(t(b"42 :*: 0x2a 0b101010 0o52"));
    assert_snapshot!(t(b"oh-no; don't let use be the def of cat!"));
}

#[test]
fn parsing() {
    fn t(script: &[u8]) -> Result<(FrozenType, Tree), ErrorList> {
        let mut global = Global::with_builtin();
        let source = global.registry.add_bytes("<test>", script.into());
        let result = parse::process(source, &mut global);
        result
            .tree
            .map(|t| (global.types.frozen(t.ty), t))
            .ok_or(result.errors)
    }

    assert_debug_snapshot!(t(b""));
    assert_debug_snapshot!(t(b"-"));
    assert_debug_snapshot!(t(b"-, split:-:, map[tonum, add1, tostr], join:+:"));
    assert_debug_snapshot!(t(b"tonum, add234121, tostr, ln"));
    assert_debug_snapshot!(t(b"[tonum, add234121, tostr] :13242:"));
    assert_debug_snapshot!(t(b"{repeat 1, {}}"));
    assert_debug_snapshot!(t(b"add 1, tostr"));
    assert_debug_snapshot!(t(b"zipwith add {1}"));
    assert_debug_snapshot!(t(b"{repeat 1, {}}"));
    assert_debug_snapshot!(t(b"{{}, repeat 1}"));
    assert_debug_snapshot!(t(b"{{42}, repeat 1}"));
    assert_debug_snapshot!(t(b"{1, 2, 3, :soleil:}, ln"));
    assert_debug_snapshot!(t(b"add 1 :2:"));
    assert_debug_snapshot!(t(b":42:, add 1"));
    assert_debug_snapshot!(t(b"add 1, tostr"));
    assert_debug_snapshot!(t(b"add 1, tonum")); // TODO: incorrect if coercion should occure
    assert_debug_snapshot!(t(b"prout 1, caca"));
    assert_debug_snapshot!(t(b"_, split_, map_, join_"));
    assert_debug_snapshot!(t(b"{const, _, add, map}"));
    assert_debug_snapshot!(t(b"2 -"));
    assert_debug_snapshot!(t(b"- 2"));
    assert_debug_snapshot!(t(b"add 1 2 3"));
    assert_debug_snapshot!(t(b"3, add 1 2"));
    assert_debug_snapshot!(t(b":1:, tostr, _"));
    assert_debug_snapshot!(t(b"iterate [_ 1, add 1] 1")); // _ :: Num -> Num -> Num
    assert_debug_snapshot!(t(b"iterate [div 1, _ 1] 1")); // _ :: Num -> Num -> Num
    assert_debug_snapshot!(t(b"iterate [div 1, add 1] 1, _")); // _ :: [Num]+ -> returnof(typeof(_))
    assert_debug_snapshot!(t(b"let {a, b} [add a b] [panic::]"));
    assert_debug_snapshot!(t(b"{1, 2, 3}, let {h,, t} h [panic::]"));
    assert_debug_snapshot!(t(b"{1, 2, 3}, let {h,, t} t [panic::]"));
    assert_debug_snapshot!(t(b"repeat 1, let {h,, t} t [panic::]"));
    assert_debug_snapshot!(t(b"let a let b a b a")); // note: is syntax error
    assert_debug_snapshot!(t(b"let {a b, c} 0"));
    assert_debug_snapshot!(t(b"[1, let 0 fst snd] 1=:a:"));
    assert_debug_snapshot!(t(b"add 1, map, flip apply {1, 2, 3}"));
    // not quite a syntax error but only out of luck:
    // ```sel
    // [let a
    //     [[let b
    //         a
    //     ]                           # b -> a
    //         [panic: unreachable:]]  # a
    // ]                               # a -> a
    //     [panic: unreachable:]       # a
    // ```
    assert_debug_snapshot!(t(b"
let a
    [let b
        a
        [panic: unreachable:]]
    [panic: unreachable:]
"));
    assert_debug_snapshot!(t(b"use :bidoof: bdf, bdf-main"));
    assert_debug_snapshot!(t(b"
def head:
    returns the first item:
    [ let {h,, t}
        h
        [panic: head on empty list:]
    ],
head {1, 2, 3}
"));
    // TODO: not passing because.. uh.. 'paramof(sum)' doesn't get assigned correctly
    assert_debug_snapshot!(t(b"
def sum:: [
    let {h,, t}
        [sum t, add h]
        0
],
{sum, _}
"));
    // TODO: not passing because of src/parse.rs:1177
    assert_debug_snapshot!(t(b"
def a:: [b 2],
def b:: [add 1],
a
"));
}

#[test]
fn interpreting() {
    fn t(script: &[u8]) -> String {
        let mut global = Global::with_builtin();
        let source = global.registry.add_bytes("<test>", script.into());
        let result = parse::process(source, &mut global);
        let mut out = Vec::new();
        interp::run_write(&result.tree.unwrap(), &global, &mut out).unwrap();
        String::from_utf8(out).unwrap()
    }

    assert_snapshot!(t(b"0"));
    assert_snapshot!(t(b"add 1 2"));
    assert_snapshot!(t(b"let a [add a 1], flip apply 2"));
}
