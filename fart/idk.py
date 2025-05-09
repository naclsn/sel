"""Usage: %s (-h|-l|-p) [<files>...]

    -h  show this help
    -l  lex the file, output tokens
    -p  parse the file, output tree
"""

__all__ = ["Apply", "END", "Kind", "lext", "parse"]

import enum
import fileinput
import re
import sys


class Kind(enum.Enum):
    word = re.compile(b"_|[-a-z]+")
    bytes = re.compile(b":([^:]|::)*:")
    number = re.compile(b"0b[01]+|0o[0-7]+|0x[0-9A-Fa-f]+|[0-9]+(\\.[0-9]+)?")
    punct = re.compile(b"[,[\\]{}=]")
    unknown = re.compile(b"[0-9A-Za-z]+|[^0-9A-Za-z]+")
    end = ""


END = Kind.end, b"", b""


def lext(s: bytes) -> tuple[Kind, bytes, bytes]:
    """kind, token, source = lext(source)"""
    s = s.lstrip()
    if not s:
        return END

    if b"#" == s[:1]:
        if b"#-" == s[:2]:
            s = s[2:]
            count = 0
            for kind, tok, s in iter(lambda: lext(s), END):
                count += (tok in b"[{") - (tok in b"}]")
                if not count:
                    break
        else:
            _, _, s = s.partition(b"\n")
        return lext(s)

    return next(
        (it, m.group(), s[m.end() :])
        for it in Kind
        if it.value and (m := it.value.match(s))
    )


type val_ = str | float | bytes | list[val_] | tuple[val_, val_] | Apply | None


class Apply:
    base: val_
    args: list[val_]


def parse_value(s: bytes) -> tuple[val_, bytes]:
    kind, tok, s = lext(s)
    fst = None
    match kind:
        case Kind.word:
            fst = str(tok)
        case Kind.bytes:
            fst = tok
        case Kind.number:
            fst = float(eval(tok))

        case Kind.punct:
            if b"[" == tok:
                fst, s = parse(s)
                kind, tok, s = lext(s)
                assert b"]" == tok

            elif b"{" == tok:
                fst = list[val_]()
                while True:
                    it, z = parse_value(s)
                    if it is None:
                        break
                    fst.append(it)
                    kind, tok, s = lext(z)
                    if b"," != tok:
                        break
                kind, tok, s = lext(s)
                assert b"}" == tok

            else:
                return None, s

        case Kind.unknown | Kind.end:
            return None, s

    kind, tok, z = lext(s)
    if b"=" == tok:
        snd, z = parse_value(z)
        return (fst, snd), z
    return fst, s


def parse_apply(s: bytes) -> tuple[val_, bytes]:
    r = Apply()

    r.base, s = parse_value(s)
    if r.base is None:
        return None, s

    r.args = []
    while True:
        it, z = parse_value(s)
        if it is None:
            break
        s = z
        r.args.append(it)

    if not r.args:
        return r.base, s
    return r, s


def parse(s: bytes) -> tuple[val_, bytes]:
    val, s = parse_apply(s)

    for kind, tok, s in iter(lambda: lext(s), END):
        if b"," != tok:
            break
        nex, s = parse_apply(s)
        if not isinstance(nex, Apply):
            base, nex = nex, Apply()
            nex.base, nex.args = base, []
        nex.args.append(val)
        val = nex

    return val, s


if "__main__" == __name__:
    flag = sys.argv.pop(1)
    if "-h" == flag:
        print(str(__doc__) % sys.argv[0])
        exit(1)

    elif "-l" == flag:
        s = b"".join(fileinput.input(mode="rb"))
        for kind, tok, s in iter(lambda: lext(s), END):
            print(kind, repr(tok))

    elif "-p" == flag:
        s = b"".join(fileinput.input(mode="rb"))
        it, s = parse_value(s)
        while it is not None:
            print(it)
            it, s = parse_value(s)
