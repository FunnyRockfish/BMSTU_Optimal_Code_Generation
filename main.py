# main.py ------------------------------------------------------------------
from frontend import Parser, lex
from cfg_from_ast import build_bb_list
from ssa import SsaBuilder
import os, sys

SRC = """
a = 1;
b = 2;
if (a > b) {
    a = a - 1;
} else {
    b = b - 1;
}
c = -1;
if (c > 0) {
    c = c - 1;
} else {
    a = 1;
}
a = 3;
return a;
"""

ast = Parser(lex(SRC)).parse()
blocks = build_bb_list(ast)

ssa = SsaBuilder(blocks)
ssa.insert_all_phi()
ssa.update_variable_versions()

ssa.print_blocks()

with open("results.dot","w",encoding="utf-8") as f:
    f.write(ssa.to_graph())

print("DOT-файл: results.dot")
