use insta::{assert_debug_snapshot, assert_snapshot};

use crate::error::ErrorList;
use crate::parse::{process, Lexer, TokenKind, Tree};
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
        let result = process(source, &mut global);
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
    assert_debug_snapshot!(t(b"iterate [div 1, _ 1] 1")); // TODO: _ :: Num -> Num -> Num
    assert_debug_snapshot!(t(b"iterate [div 1, add 1] 1, _")); // _ :: [Num]+ -> returnof(typeof(_))
    assert_debug_snapshot!(t(b"let {a, b} [add a b]"));
    assert_debug_snapshot!(t(b"{1, 2, 3}, let {h,, t} h"));
    assert_debug_snapshot!(t(b"{1, 2, 3}, let {h,, t} t"));
    assert_debug_snapshot!(t(b"repeat 1, let {h,, t} t"));
    assert_debug_snapshot!(t(b"let a let b a"));
}
