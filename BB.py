from dataclasses import dataclass

ALLOCA, LOAD, STORE, BR, CONDBR, ICMP, MUL, ADD, SUB, RET, PHI = \
    'alloca load store br condbr icmp mul add sub ret phi'.split()

class Value: pass

@dataclass
class Variable(Value):
    name: str
    version: int = 0
    is_temp: bool = False
    def __str__(self):
        return f'{self.name}({self.version})' if not self.is_temp else self.name
    __repr__ = __str__
    def __hash__(self):
        return hash((self.name, self.version))

@dataclass
class IntConst(Value):
    value: int
    def __str__(self):
        return str(self.value)
    __repr__ = __str__

@dataclass
class Instruction:
    typ: str
    args: dict
    def __str__(self):
        t = self.typ
        a = self.args
        if t == STORE:
            return f'{a["to"]} <- {a["from"]}'
        if t == LOAD:
            return f'{a["to"]} <- {a["from"]}'
        if t in (ADD, SUB, MUL):
            return f'{a["to"]} <- {a["oper1"]} {t} {a["oper2"]}'
        if t == ICMP:
            return f'{a["to"]} <- {a["arg1"]} > {a["arg2"]}'
        if t == BR:
            return f'go to BLOCK{a["dest"]}'
        if t == CONDBR:
            return f'if (!{a["cond"]}) go to BLOCK{a["dest1"]} else go to BLOCK{a["dest2"]}'
        if t == PHI:
            return f'{a["to"]} = phi({", ".join(map(str, a["from"]))})'
        if t == ALLOCA:
            return f'new variable {a["name"]}'
        if t == RET:
            return f'ret: value {a["value"]}'
        return f'{t}: {a}'
    __repr__ = __str__

class BB:
    def __init__(self):
        self.block_num = 0
        self.instructions = []
        self.returned = False
        self.variables = {}
        self.varcounter = 0

    def __hash__(self):
        return self.block_num

    def __str__(self):
        body = '\n'.join(f'    {instr}' for instr in self.instructions)
        return f'BLOCK {self.block_num}' + '{\n' + body + '\n}'
    __repr__ = __str__

    def add_instr(self, instr):
        if not self.returned:
            self.instructions.append(instr)

    def create_tmp_var(self):
        name = f'tmp_{self.block_num}_{self.varcounter}'
        self.varcounter += 1
        return Variable(name, 0, True)

    def alloca_variable(self, name):
        v = Variable(name, 0)
        self.variables[name] = v
        self.add_instr(Instruction(ALLOCA, {"name": name}))
        return v

    def new_break(self, dest):
        self.add_instr(Instruction(BR, {"dest": dest.block_num}))

    def new_cond_break(self, cond, dest1, dest2):
        self.add_instr(Instruction(CONDBR, {"cond": cond, "dest1": dest1.block_num, "dest2": dest2.block_num}))

    def get_edges(self):
        if not self.instructions:
            return set()
        last = self.instructions[-1]
        if last.typ == BR:
            return {(self.block_num, last.args["dest"])}
        if last.typ == CONDBR:
            return {(self.block_num, last.args["dest1"]), (self.block_num, last.args["dest2"])}
        return set()

    def build_changing_variables(self):
        self.changing_variables = {i.args['to'] for i in self.instructions if i.typ == STORE}
        self.phi_var_blocks = {}
