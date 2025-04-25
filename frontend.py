# frontend.py --------------------------------------------------------------
import re
from collections import namedtuple

Token = namedtuple("Token", ["type", "value"])

TOKEN_SPEC = [
    ("NUMBER",  r"\d+"),
    ("IDENT",   r"[A-Za-z_]\w*"),
    ("EQ",      r"=="),
    ("NE",      r"!="),
    ("LE",      r"<="),
    ("GE",      r">="),
    ("AND",     r"&&"),
    ("OR",      r"\|\|"),
    ("ASSIGN",  r"="),
    ("OP",      r"[+\-*/<>]"),
    ("NOT",     r"!"),
    ("LPAREN",  r"\("),
    ("RPAREN",  r"\)"),
    ("LBRACE",  r"\{"),
    ("RBRACE",  r"\}"),
    ("SEMI",    r";"),
    ("SKIP",    r"[ \t\r\n]+"),
]
_master = re.compile("|".join(f"(?P<{n}>{p})" for n, p in TOKEN_SPEC))


def lex(text):
    for m in _master.finditer(text):
        if m.lastgroup != "SKIP":
            yield Token(m.lastgroup, m.group())


# ---------- AST узлы ------------------------------------------------------
class AST: pass


class Program(AST):
    def __init__(self, stmts):
        self.stmts = stmts


class Assign(AST):
    def __init__(self, name, expr):
        self.name = name
        self.expr = expr


class If(AST):
    def __init__(self, cond, then_s, else_s):
        self.cond = cond
        self.then_s = then_s
        self.else_s = else_s


class While(AST):
    def __init__(self, cond, body):
        self.cond = cond
        self.body = body


class BinOp(AST):
    def __init__(self, op, left, right):
        self.op = op
        self.left = left
        self.right = right


class UnOp(AST):
    def __init__(self, op, expr):
        self.op = op
        self.expr = expr


class Var(AST):
    def __init__(self, name):
        self.name = name


class Num(AST):
    def __init__(self, value):
        self.value = int(value)

class Return(AST):
    def __init__(self, expr):
        self.expr = expr



# ---------- Парсер --------------------------------------------------------
class Parser:
    def __init__(self, tokens):
        self.tokens = list(tokens)
        self.i = 0

    def cur(self):
        return self.tokens[self.i] if self.i < len(self.tokens) else Token("EOF", "")

    def eat(self, *expected_types):
        tok = self.cur()
        if expected_types and tok.type not in expected_types:
            raise SyntaxError(f"Expected {expected_types}, got {tok}")
        self.i += 1
        return tok

    def stmt(self):
        tok = self.cur()
        if tok.type == "IDENT" and self.tokens[self.i + 1].type == "ASSIGN":
            return self.assign()
        if tok.type == "IDENT" and tok.value == "if":
            return self.if_stmt()
        if tok.type == "IDENT" and tok.value == "while":
            return self.while_stmt()
        if tok.type == "IDENT" and tok.value == "return":
            return self.return_stmt()
        raise SyntaxError(f"Unknown statement start: {tok}")

    def parse(self):
        stmts = []
        while self.cur().type != "EOF":
            stmts.append(self.stmt())
        return Program(stmts)


    def return_stmt(self):
        self.eat("IDENT")  # return
        expr = self.expr()
        self.eat("SEMI")
        return Return(expr)

    def assign(self):
        name = self.eat("IDENT").value
        self.eat("ASSIGN")
        expr = self.expr()
        self.eat("SEMI")
        return Assign(name, expr)

    def if_stmt(self):
        self.eat("IDENT")  # if
        self.eat("LPAREN")
        cond = self.expr()
        self.eat("RPAREN")
        self.eat("LBRACE")
        then_block = []
        while self.cur().type != "RBRACE":
            then_block.append(self.stmt())
        self.eat("RBRACE")
        self.eat("IDENT")  # else
        self.eat("LBRACE")
        else_block = []
        while self.cur().type != "RBRACE":
            else_block.append(self.stmt())
        self.eat("RBRACE")
        return If(cond, then_block, else_block)

    def while_stmt(self):
        self.eat("IDENT")  # while
        self.eat("LPAREN")
        cond = self.expr()
        self.eat("RPAREN")
        self.eat("LBRACE")
        body = []
        while self.cur().type != "RBRACE":
            body.append(self.stmt())
        self.eat("RBRACE")
        return While(cond, body)

    # ---- выражения с приоритетами ----------------------------------------
    def expr(self):
        return self.or_()

    def or_(self):
        node = self.and_()
        while self.cur().type == "OR":
            self.eat("OR")
            node = BinOp("||", node, self.and_())
        return node

    def and_(self):
        node = self.eq()
        while self.cur().type == "AND":
            self.eat("AND")
            node = BinOp("&&", node, self.eq())
        return node

    def eq(self):
        node = self.rel()
        while self.cur().type in ("EQ", "NE"):
            op = self.eat("EQ", "NE").value
            node = BinOp(op, node, self.rel())
        return node

    def rel(self):
        node = self.add()
        while self.cur().type in ("OP", "LE", "GE"):
            if self.cur().type == "OP" and self.cur().value in ("<", ">"):
                op = self.eat("OP").value
            else:
                op = self.eat("LE", "GE").value
            node = BinOp(op, node, self.add())
        return node

    def add(self):
        node = self.mul()
        while self.cur().type == "OP" and self.cur().value in ("+", "-"):
            op = self.eat("OP").value
            node = BinOp(op, node, self.mul())
        return node

    def mul(self):
        node = self.unary()
        while self.cur().type == "OP" and self.cur().value in ("*", "/"):
            op = self.eat("OP").value
            node = BinOp(op, node, self.unary())
        return node

    def unary(self):
        if self.cur().type == "OP" and self.cur().value == "-":
            self.eat("OP")
            return UnOp("-", self.unary())
        if self.cur().type == "NOT":
            self.eat("NOT")
            return UnOp("!", self.unary())
        return self.primary()

    def primary(self):
        tok = self.cur()
        if tok.type == "NUMBER":
            return Num(self.eat("NUMBER").value)
        if tok.type == "IDENT":
            return Var(self.eat("IDENT").value)
        if tok.type == "LPAREN":
            self.eat("LPAREN")
            node = self.expr()
            self.eat("RPAREN")
            return node
        raise SyntaxError(f"Bad primary: {tok}")
