import json

with open("Opcodes.json") as f:
    opcodes = json.load(f)

un_op = opcodes['unprefixed']



def main(argv):
    line = list(map(lambda x: x.upper(), argv[1:]))
    disasm = ""
    # line = list(map(lambda s: "0x"+s, line))
    # breakpoint()
    if line and "0x"+line[0] in un_op:
        op = un_op["0x"+line[0]]
        disasm += op['mnemonic'].lower() + " "
        
        if op['bytes'] > len(line):
            print(f"what the fuck. {line} should be {op}")
            return(-1)
        imm8 = int(line[1], base=16) if op['bytes'] >= 2 else None
        imm16= int(line[2]+line[1], base=16) if op['bytes'] == 3 else None
        
        ret_val = len(op['operands'])
        for operand in op['operands']:
            if operand['immediate'] == False:
                fmt = "(%s)"
            else:
                fmt = "%s"
            if 'r8' == operand['name']:
                s = "$%02x"%(imm8 if imm8<128 else imm8-256)
            elif 'a8' == operand['name']:
                s = "$ff00+$%02x"%imm8
            elif '8' in operand['name']:
                s = "$%02x"%imm8
            elif '16' in operand['name']:
                s = "$%04x"%imm16
            else:
                s = (operand['name'])
            disasm += (fmt%s).lower() + " "
    else:
        disasm = "unknown `%s`"%line[0]
        ret_val = -2
    print(disasm, end='')
    
    return ret_val

from sys import argv
if __name__ == "__main__":
    if len(argv) == 1:
        print("usage: %s opcode"%argv[0])
        exit(-1)
    else:
        # breakpoint()
        exit(main(argv))
