# ir.py --------------------------------------------------------------------
from BB import *
from frontend import Num, Var, BinOp, UnOp

class TmpGen:
    def __init__(s,bb): s.bb=bb
    def new(s): return s.bb.create_tmp_var()

def lower_expr(e, bb, tgen):
    code=[]
    if isinstance(e, Num):  return IntConst(e.value), code
    if isinstance(e, Var):
        tmp=tgen.new()
        bb.add_instr(Instruction(LOAD,{"from":bb.variables[e.name],"to":tmp}))
        return tmp, code
    if isinstance(e, UnOp):
        val,c1=lower_expr(e.expr,bb,tgen); code+=c1
        if e.op=="-":
            zero=IntConst(0); tmp=tgen.new()
            code.append(Instruction(SUB,{"oper1":zero,"oper2":val,"to":tmp}))
            return tmp,code
        if e.op=="!":
            zero=IntConst(0); tmp=tgen.new()
            code.append(Instruction(ICMP,{"arg1":val,"arg2":zero,"to":tmp}))
            return tmp,code
    if isinstance(e,BinOp):
        l,c1=lower_expr(e.left,bb,tgen); r,c2=lower_expr(e.right,bb,tgen); code+=c1+c2
        tmp=tgen.new()
        op=e.op
        if op=="+": kind=ADD
        elif op=="-": kind=SUB
        elif op=="*": kind=MUL
        else: kind=ICMP
        if kind in (ADD,SUB,MUL):
            code.append(Instruction(kind,{"oper1":l,"oper2":r,"to":tmp}))
        else:
            code.append(Instruction(kind,{"arg1":l,"arg2":r,"to":tmp}))
        return tmp,code
    raise RuntimeError(e)
