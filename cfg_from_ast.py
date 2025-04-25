# cfg_from_ast.py ----------------------------------------------------------
from BB import *
from ir import lower_expr, TmpGen
from frontend import Assign, If, While

def build_bb_list(ast_prog):
    blocks=[]; counter=0
    def new_bb():
        nonlocal counter
        b=BB(); b.block_num=counter; counter+=1; blocks.append(b); return b

    def walk(stmts, cur, follow):
        tg=TmpGen(cur)
        for s in stmts:
            if isinstance(s,Assign):
                if s.name not in cur.variables: cur.alloca_variable(s.name)
                rhs,code=lower_expr(s.expr,cur,tg)
                for inst in code: cur.add_instr(inst)
                cur.add_instr(Instruction(STORE,{"from":rhs,"to":cur.variables[s.name]}))
            elif isinstance(s,If):
                then_bb,else_bb,merge=new_bb(),new_bb(),new_bb()
                # cond
                cond,code=lower_expr(s.cond,cur,tg)
                for i in code: cur.add_instr(i)
                cur.new_cond_break(cond,then_bb,else_bb)
                walk(s.then_s,then_bb,merge); walk(s.else_s,else_bb,merge)
                cur=merge
            elif isinstance(s,While):
                head,body,after=new_bb(),new_bb(),new_bb()
                cur.new_break(head)
                cond,code=lower_expr(s.cond,head,TmpGen(head))
                for i in code: head.add_instr(i)
                head.new_cond_break(cond,body,after)
                walk(s.body,body,head)
                cur=after
        if follow and cur is not follow: cur.new_break(follow)
    entry=new_bb()
    walk(ast_prog.stmts, entry, None)
    return blocks
