#! /usr/bin/python

import sys
import io
import os
import struct
import argparse

from vm6101def import *

#-------------------------------------------------------------------------------------------------------------

def dasm(code, lit_fmt = "H", show_adress = True, skip_prefix = True) :
	ops = { opc : (name[3 if skip_prefix else 0:], arg) for name, (opc, arg) in asm.items() }
	registers = { nr : name[2 if skip_prefix else 0:] for name, nr in regs.items() }
        out = []
	word_sz = len(struct.pack(lit_fmt, 0))
        adr_fmt = "0x%0" + ("%i" % (2 * word_sz)) + "x: "
        i = 0
        while i < len(code) :
                if show_adress :
                        out.append(adr_fmt % i)
                opcode = ord(code[i])
                if not opcode in ops :
	                out.append("?")
                else :
                        op_name, args = ops[opcode]
                        out.append("%-8s " % op_name)
                        if args == OPARG_NONE:
                                pass
                        elif args == OPARG_BYTE :
                                a = code[i+1:i+2]
                                i += 1
                                out.append(hex(struct.unpack("B", a)[0]))
                        elif args == OPARG_1WORD :
                                a = code[i+1:i+1+word_sz]
                                i += word_sz
                                out.append(hex(struct.unpack(lit_fmt, a)[0]))
                        elif args == OPARG_DWORD :
                                a = code[i+1:i+1+(word_sz*2)]
                                i += word_sz
                                out.append(hex(struct.unpack(lit_fmt, a)[0]))
                        elif args == OPARG_REG :
                                a = code[i+1:i+1+word_sz]
                                i += word_sz
                                out.append(registers[struct.unpack(lit_fmt, a)[0]])
                        else :
                                raise Exception("evil")
                i += 1
                out.append(os.linesep)
        return "".join(out)

#-------------------------------------------------------------------------------------------------------------

if __name__ == "__main__" :

        wd = os.getcwd()
        argp = argparse.ArgumentParser(description="VM6101 disassembler")
        argp.add_argument("source",
                          help = "input file")
        argp.add_argument("-o", "--out", dest="output",
                          default = None,
                          help = "output file name")
        #TODO endianness
        #TODO word size
        args = argp.parse_args()

        try :
                f = io.open(args.source, "rb")
                blob = f.read()
                f.close()
        except :
                print("error while reading input file")
                sys.exit(1)
                
        out = dasm(blob)

	if args.output :
	        try :
         	       f = io.open(args.output, "w")
         	       f.write(out)
         	       f.close()
	        except :
        	        print("error while writing output file")
        	        sys.exit(2)
	else :
	        print(out)

        sys.exit(0)

#-------------------------------------------------------------------------------------------------------------

