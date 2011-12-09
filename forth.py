#! /usr/bin/python2.6

import sys
import io
import re
import os
import itertools
from itertools import takewhile, product, ifilter, imap
from struct import pack, unpack
import argparse
import binascii
from collections import namedtuple
import pprint

from vm6101def import *

#-------------------------------------------------------------------------------------------------------------

optimize_t = namedtuple("optimize_t", ["short_lit", ]) #TODO const_folding push_joining

o = optimize_t(short_lit=True)

fmtop0 = "<B"
nfmtop0 = "B"
fmtop1b = "<BB"
nfmtop1b = "BB"
fmt1b = "<B"
nfmt1b = "B"

word_mask = 0xffff

def environ_init(word_size, endianness, heap_offset, heap_length) :
	global fmtop1w, fmtop1uw, fmt1w, fmt1uw, nfmtop1w, nfmtop1uw, nfmt1w, nfmt1uw
	global word_mask, heap_size, heap_start
	heap_start = heap_offset
	heap_size = heap_length
	if endianness == "little" :
		end = "<"
	elif endianness == "big" :
		end = ">"
	else :
		raise Exception("unsupported endianness")
	if word_size == 16 :
		fmtop1w = end + "Bh"
		fmtop1uw = end + "BH"
		fmt1w = end + "h"
		fmt1uw = end + "H"
		nfmtop1w = "Bh"
		nfmtop1uw = "BH"
		nfmt1w = "h"
		nfmt1uw = "H"
	elif word_size == 32 :
		fmtop1w = end + "Bi"
		fmtop1uw = end + "BI"
		fmt1w = end + "i"
		fmt1uw = end + "I"
		nfmtop1w = "Bi"
		nfmtop1uw = "BI"
		nfmt1w = "i"
		nfmt1uw = "I"
	else :
		raise Exception("unsupported word size")
	word_mask = (2 ** word_size) - 1

#-------------------------------------------------------------------------------------------------------------

def mm(addr) :
	return addr | (1 << ((len(pack(fmt1uw, 0)) << 3) - 1))

#-------------------------------------------------------------------------------------------------------------

include_info = namedtuple("include", ["dir", "fname", "pos"])
source_info = namedtuple("source", ["fname", "string"])
tok_info = namedtuple("tok", ["token", "start", "end", "file", "line", "column"])

def tok_wrap(t, toks, cmnts, lines, fname) :
	lns = ifilter(lambda ln: ln[1] <= t.end(), lines)
	lnnr = reduce(lambda a, b: a + 1, lns, 1)
	return tok_info(t.group(0), t.start(), t.end(), fname,
		lnnr, 666)#t.start() - lns[-1].start())

def tok_is_commented(t, cmnts) :
	return reduce(lambda b,c: b and not (t.start() >= c[0] and t.start() < c[1]), cmnts, True)

def pp_tokenize(src, fname) :
	""" process source, outputs list of tok_info objects """
	cmnts = map(lambda c: (c.start(), c.end()), re_cmnt.finditer(src))#XXX something more hitech?
	toks = re_tok.finditer(src)
	lines = map(lambda c: (c.start(), c.end()), re_newln.finditer(src))
	return map(lambda t: tok_wrap(t, toks, cmnts, lines, fname),
		ifilter(lambda t: tok_is_commented(t, cmnts), toks))

def pp_dependencies(src_tokens) :
	""" extracts include/require directives, outputs list of include_info tuples """
	included = []
	for i in range(0, len(src_tokens)) :
		tl = src_tokens[i].token.lower()
		if tl in ("include", "require") :
			included.append(include_info(tl, src_tokens[i + 1].token, i))
	return included

def pp_include(order, sources) :
	""" join sources into single list of tok_info objects """
	src_files = dict(sources)
	out = []
	included = []
	while order :
		fname, includes = order.pop()
		fsrc = src_files[fname]
		for incl in includes :
			if incl.dir == "include" or (incl.dir == "require" and not incl.fname in included) :
				included.append(fname)
				fsrc[incl.pos:incl.pos] = src_files[incl.fname]
		out.extend(fsrc)
	return out

def pp_depends(d, ordered) :
	o = dict(ordered)
	return (len(d[1]) == 0) or all([(f[1] in o) for f in d[1]])

def preproc(sources) :
	""" process list of source strings into single list of tok_info objects """
	src_tokens = [(src.fname, pp_tokenize(src.string, src.fname)) for src in sources]
	dependencies = [(toks[0], pp_dependencies(toks[1])) for toks in src_tokens]
	order = []
	while dependencies :
		indep = [a for a in ifilter(lambda d: pp_depends(d, order), dependencies)]
		order.extend(indep)
		for d in indep :
			dependencies.remove(d)
	return pp_include(order, src_tokens)

def preproc_init(newline) :
	global re_newln, re_cmnt, re_tok
	re_newln = re.compile(newline)
	#TODO use r" so \\\\ becomes \
	#re_cmnt = re.compile("\([^\(\)]*?\)|\\\\\\s.*?" + newline)
	re_cmnt = re.compile(r'\([^()]*?\)|\\\s.*?' + newline)#"\([^\(\)]*?\)|\\\\\\s.*?" + newline)
	#re_tok = re.compile("[cCsS]?\"[^\"]*?\"|\S+")#re.compile("\S+") / "[cCsS\s]?\"[^\"]*?\"|\S+"
	re_tok = re.compile('[cCsS]?"[^"]*?"|\S+')#"[cCsS]?\"[^\"]*?\"|\S+")#re.compile("\S+") / "[cCsS\s]?\"[^\"]*?\"|\S+"

#-------------------------------------------------------------------------------------------------------------

class fth_word :
	def __init__(self, name, is_inline, blob) :
		self.name = name
		self.is_inline = is_inline
		self.blob = blob
		self.adress = None
		self.used = []

#-------------------------------------------------------------------------------------------------------------

def outp_lbl_to_adr(out, lbl) :
	return reduce(lambda a, b: a + len(b), out[0:lbl], 0)

#-------------------------------------------------------------------------------------------------------------

def ctrlIf(tokens, out, cstack, rstack, dictionary, data, lvars) :
	out.append(pack(fmtop0, OP_CRJMP))
	cstack.append(("if", len(out)))
	out.append(pack(fmt1w, 0))

def ctrlThen(tokens, out, cstack, rstack, dictionary, data, lvars) :
	tok, lbl = cstack.pop()
	if not tok in ("if", "else") :
		raise Exception("syntax error")
	out[lbl] = pack(fmt1w, outp_lbl_to_adr(out, len(out)) - outp_lbl_to_adr(out, lbl) + 1)

def ctrlElse(tokens, out, cstack, rstack, dictionary, data, lvars) :
	tok, lbl = cstack.pop()
	if tok != "if" :
		raise Exception("syntax error")
	out.append(pack(fmtop0, OP_RJMP))
	cstack.append(("else", len(out)))
	out.append(pack(fmt1w, 0))
	out[lbl] = pack(fmt1w, outp_lbl_to_adr(out, len(out)) - outp_lbl_to_adr(out, lbl))

#-------------------------------------------------------------------------------------------------------------

def ctrlCase(tokens, out, cstack, rstack, dictionary, data, lvars) :
	cstack.append(("case", ))

def ctrlOf(tokens, out, cstack, rstack, dictionary, data, lvars) :
	out.append(pack(fmtop0 * 2, OP_OVER, OP_CE))
	ctrlIf(tokens, out, cstack, rstack, dictionary, data, lvars)

def ctrlEndOf(tokens, out, cstack, rstack, dictionary, data, lvars) :
	ctrlElse(tokens, out, cstack, rstack, dictionary, data, lvars)

def ctrlEndCase(tokens, out, cstack, rstack, dictionary, data, lvars) :
	while cstack[len(cstack) - 1][0] != "case" :
		ctrlThen(tokens, out, cstack, rstack, dictionary, data, lvars)
	cstack.pop()

#-------------------------------------------------------------------------------------------------------------
	
def ctrlLeave(tokens, out, cstack, rstack, dictionary, data, lvars) :
	if not reduce(lambda a, b: a or b[0] in ("do", "begin", ), cstack, False) :
		raise Exception("syntax error")
	for ctrl in cstack[::-1] :
		if ctrl[0] in ("begin",) : # TODO
			out.append(pack(fmtop0, OP_RJMP))
			ctrl.append(("leave", len(out)))
			out.append(pack(fmt1w, 0))
			return None
	raise Exception("syntax error")
	

def ctrlBegin(tokens, out, cstack, rstack, dictionary, data, lvars) :
	cstack.append(["begin", len(out)])

def ctrlUntil(tokens, out, cstack, rstack, dictionary, data, lvars) :
	tok, lbl = cstack.pop()
	if tok != "begin" :
		raise Exception("syntax error")
	out.append(pack(fmtop1w, OP_CRJMP, outp_lbl_to_adr(out, lbl) - outp_lbl_to_adr(out, len(out))))

def ctrlAgain(tokens, out, cstack, rstack, dictionary, data, lvars) :
	ctrl = cstack.pop()
	if ctrl[0] != "begin" :
		raise Exception("syntax error")
	lbl = ctrl[1]
	out.append(pack(fmtop1w, OP_RJMP, outp_lbl_to_adr(out, lbl) - outp_lbl_to_adr(out, len(out))))
	leaves = ctrl[2:]
	if leaves and all(map(lambda l : l[0] == "leave", leaves)) :
		for leave in leaves :
			lbl = leave[1]
			out[lbl] = pack(fmt1w,
				outp_lbl_to_adr(out, lbl) - outp_lbl_to_adr(out, len(out)))

def ctrlWhile(tokens, out, cstack, rstack, dictionary, data, lvars) :
	out.append(pack(fmtop0, OP_CRJMP))
	cstack.append(("while", len(out)))
	out.append(pack(fmt1w, 0))

def ctrlRepeat(tokens, out, cstack, rstack, dictionary, data, lvars) :
	tokw, lblw = cstack.pop()
	tokb, lblb = cstack.pop()
	if tokw != "while" or tokb != "begin":
		raise Exception("syntax error")
	out.append(pack(fmtop1w, OP_RJMP, outp_lbl_to_adr(out, lblb) - outp_lbl_to_adr(out, len(out))))
	out[lbl] = pack(fmt1w, outp_lbl_to_adr(out, len(out)) - outp_lbl_to_adr(out, lblw))

#-------------------------------------------------------------------------------------------------------------

def ctrlDo(tokens, out, cstack, rstack, dictionary, data, lvars) :
	out.append(pack(fmtop0 + (nfmtop0 * 2), OP_SWAP, OP_TOR, OP_TOR))
	cstack.append(["do", len(out)])

def ctrlQuestionDo(tokens, out, cstack, rstack, dictionary, data, lvars) : #TODO optimize
	out.append(pack(fmtop0 + (nfmtop0 * 4), OP_OVER, OP_OVER, OP_SWAP, OP_TOR, OP_TOR))
	out.append(pack(fmtop0 + (nfmtop0 * 2), OP_CE, OP_NOT, OP_CRJMP))
	cstack.append(["?do", len(out)])
	out.append(pack(fmt1w, 0))

def ctrlLoop(tokens, out, cstack, rstack, dictionary, data, lvars) :
	out.append(pack(fmtop1w, OP_PUSH, 1))
	ctrlPlusLoop(tokens, out, cstack, rstack, dictionary, data, lvars)

def ctrlPlusLoop(tokens, out, cstack, rstack, dictionary, data, lvars) :
	ctrl = cstack.pop()
	if not (ctrl[0] in ("do", "?do")) :
		raise Exception("syntax error")
	lbl = outp_lbl_to_adr(out, ctrl[1]) - outp_lbl_to_adr(out, len(out))
	if ctrl[0] == "?do" :
		out[ctrl[1]] = pack(fmt1w, -lbl + (2 * len(pack(fmt1w, 0))))
		lbl += len(pack(fmt1w, 0))
	out.append(pack(fmtop1w, OP_LOOP, lbl))
	out.append(pack(fmtop0 + (nfmtop0 * 3), OP_FROMR, OP_DROP, OP_FROMR, OP_DROP))

def ctrlI(tokens, out, cstack, rstack, dictionary, data, lvars) :
	out.append(pack(fmtop0, OP_RFETCH))

def ctrlJ(tokens, out, cstack, rstack, dictionary, data, lvars) :
	raise Exception("not implemented")

#-------------------------------------------------------------------------------------------------------------

def ctrlCreate(tokens, out, cstack, rstack, dictionary, static, lvars) :
	name = tokens.pop(0).token.lower()
	adr = mm((heap_start + heap_size - sum(map(lambda slot: slot[0], static)) -
		len(pack(fmt1uw, 0))))
	blob = pack(fmtop1uw + nfmtop0, OP_PUSH, adr, OP_FETCHW)
	static.append((len(pack(fmt1uw, 0)), ))
	out.append(pack(fmtop1uw + nfmtop1uw + nfmtop0,
		OP_FETCHREG, R_HEAPB, OP_PUSH, adr, OP_STOREW))
	dictionary[name] = fth_word(name, True, blob)

#-------------------------------------------------------------------------------------------------------------

def ctrlVariable(tokens, out, cstack, rstack, dictionary, static, lvars) :
	name = tokens.pop(0).token.lower()
	adr = mm((heap_start + heap_size - sum(map(lambda slot: slot[0], static)) -
		len(pack(fmt1uw, 0))))
	blob = pack(fmtop1uw, OP_PUSH, adr)
	static.append((len(pack(fmt1uw, 0)), ))
	out.append(blob)
	dictionary[name] = fth_word(name, True, blob, None, None)

def ctrl2Variable(tokens, out, cstack, rstack, dictionary, static, lvars) :
	name = tokens.pop(0).token.lower()
	size = 2 * len(pack(fmt1uw, 0))
	adr = mm((heap_start + heap_size - sum(map(lambda slot: slot[0], static)) - size))
	blob = pack(fmtop1uw, OP_PUSH, adr)
	static.append((size, ))
	out.append(blob)
	dictionary[name] = fth_word(name, True, blob, None, None)

def ctrlValue(tokens, out, cstack, rstack, dictionary, static, lvars) :
	name = tokens.pop(0).token.lower()
	adr = mm((heap_start + heap_size - sum(map(lambda slot: slot[0], static)) -
		len(pack(fmt1uw, 0))))
	static.append((len(pack(fmt1uw, 0)), ))
	out.append(pack(fmtop1uw + nfmtop0, OP_PUSH, adr, OP_STOREW))
	dictionary[name] = fth_word(name, True,
		pack(fmtop1uw + nfmtop0, OP_PUSH, adr, OP_FETCHW))#XXX XXX XXX

lvar_t = namedtuple("lvar", ["index","f_rel_addr", "blob"])

def ctrlTo(tokens, out, cstack, rstack, dictionary, static, lvars) :
	name = tokens.pop(0).token.lower()
	if lvars and name in lvars :
		out.append(pack(fmtop1uw + nfmtop0, OP_PUSH, lvars[name].index + 1, OP_FSET))
	else :
		out.append(pack(fmtop0, OP_PUSH))
		out.append(dictionary[name].blob[1:(1 + len(pack(fmt1uw, 0)))])#XXX evil!!!
		out.append(pack(fmtop0, OP_STOREW))

def ctrlLocals(tokens, out, cstack, rstack, dictionary, static, lvars) :
	if lvars :
		raise Exception("only one locals block per word allowed")
	tok = None
	while tok != "|" : #XXX takewhile?
		tinfo = tokens.pop(0)
		tok = tinfo.token
		if tok == "|" :
			break
		adr = (len(lvars) + 1) * len(pack(fmt1uw, 0))
		lvars[tinfo.token.lower()] = lvar_t(len(lvars), adr,
			pack(fmtop1uw + nfmtop0, OP_PUSH, len(lvars) + 1, OP_FPICK))
	out.append(pack(fmtop0, OP_TOR) * len(lvars))

#-------------------------------------------------------------------------------------------------------------

def ctrlTick(tokens, out, cstack, rstack, dictionary, static, lvars) :
	name = tokens.pop(0).token.lower()
	word = dictionary[name]
	use_word(word, dictionary, ": tick")
	out.append(pack(fmtop1uw, OP_PUSH, word.adress))

#-------------------------------------------------------------------------------------------------------------

def ctrlChar(tokens, out, cstack, rstack, dictionary, static, lvars) :
	s = tokens.pop(0).token.decode("string-escape")
	out.append(pack(fmtop1uw, OP_PUSH, ord(s)))

#-------------------------------------------------------------------------------------------------------------

statements = {
	"'" : ctrlTick, "if" : ctrlIf, "then" : ctrlThen, "else" : ctrlElse,
	"begin" : ctrlBegin, "until" : ctrlUntil, "do" : ctrlDo, "loop" : ctrlLoop,
	"+loop" : ctrlPlusLoop, "?do" : ctrlQuestionDo, "leave" : ctrlLeave,
	"i" : ctrlI, "j" : ctrlJ, "again" : ctrlAgain, "while" : ctrlWhile,
	"repeat" : ctrlRepeat, "case" : ctrlCase, "of" : ctrlOf, "endof" : ctrlEndOf,
	"endcase" : ctrlEndCase, "variable" : ctrlVariable,
	"2variable" : ctrl2Variable, "create" : ctrlCreate, "value" : ctrlValue,
	"to" : ctrlTo, "locals|" : ctrlLocals, "[char]" : ctrlChar
}

#-------------------------------------------------------------------------------------------------------------

def assign_word_adress(word, dictionary) :
	#global dictionary_offset
	words = filter(lambda w: not w[1].is_inline and word != w[1] and len(w[1].used),
		dictionary.items())
	if any(map(lambda w: w[1].adress == None, words)) :
		raise Exception("can not assign adress to word " + word.name)
	return reduce(lambda adr, word: adr + len(word[1].blob), words, dictionary_offset)	

def use_word(word, dictionary, caller) :
	if word.is_inline :
		raise Exception("misuse")
	word.used.append(caller)
	if not word.is_inline and word.adress == None :
		word.adress = assign_word_adress(word, dictionary)

#-------------------------------------------------------------------------------------------------------------

def parse_lit(tok, is_asm) :
	if tok.endswith("\"") :
		if tok.startswith("\"") :
			return tok[1:len(tok)-1].decode("string-escape")
		elif tok.startswith("s\"") :
			return tok[2:len(tok)-1].decode("string-escape")
	else :
		try :
			return int(tok, 0)
		except ValueError :
			return None

#-------------------------------------------------------------------------------------------------------------

class CompileContext() :
	def __init__(self) :
		self.tokens = None
		self.out = None
		self.cstack = None
		self.rstack = None
		self.dictionary = None
		self.static = None
		self.lvars = None
		self.data = None

#-------------------------------------------------------------------------------------------------------------

# TODO export symbols (variables, words, locals, line number to ip value)
# TODO redefinition is error (thus staments dict can be merged with dictionary, somehow)

def incforth_compile(tokens, dictionary) :
	ppstack = [True, ]
	cstack = []
	rstack = []
	static = []
	data = []
	lvars = None
	blob_main = []
	compiling_word = False
	word_name = None
	blob_word = None
	out = blob_main
	is_asm = False

	while len(tokens) :
		
		tinfo = tokens.pop(0)
		#print tinfo
		rtok = tinfo.token
		tok = rtok.lower()

		if tok == "[if]" :
			ppstack.apppend(bool(struct.unpack(fmt1w + nfmtop0, out.pop())))#XXX holy shit!
		elif tok == "[ifdef]" :
			ppstack.apppend(tokens.pop(0).token.lower() in dictionary)
		elif tok == "[ifndef]" :
			ppstack.apppend(not (tokens.pop(0).token.lower() in dictionary))
		elif tok == "[else]" :
			ppstack[-1] = not ppstack[-1]
		elif tok in ("[then]", "[endif]", ) : #TODO some checks should be nice
			ppstack.pop()
		elif not all(ppstack) :
			continue
		
		if tok in (":", ":noname", ) : #TODO invoke using statements
			if cstack and tok == ":":
				raise Exception("unfinished control structure")
			lvars = {}
			compiling_word = True
			blob_word = []
			out = blob_word
			is_inline = False
			is_asm = False
			word_name = tokens.pop(0).token.lower()
			continue

		if compiling_word and tok == "inline" : #TODO invoke using statements
			is_inline = True
			continue

#TODO
		if tok == "immediate" :
			#last_word.is_inline = True
			continue

		if compiling_word and tok == "asm" : #TODO invoke using statements
			is_asm = True
			continue

		if tok == ";" : #TODO invoke using statements
			# TODO check content of cstack - should be empty
			compiling_word = False
			lvars = None
			local = None
			if not is_inline :
				out.append(pack(fmtop0, OP_RETURN))
			if word_name == ":noname" :
				word_name += " " + reduce(lambda x:
					1 if dictionary.keys().startswith(":noname") else 0, 0)
				w = fth_word(word_name, is_inline, "".join(out))
				dictionary[word_name] = w
				use_word(w, dictionary, "n/a")
				out.append(pack(fmtop1w, OP_PUSH, w.adress))
			else :
				if cstack and tok == ":":
					raise Exception("unfinished control structure")
				dictionary[word_name] = fth_word(word_name, is_inline, "".join(out))
			blob_word = None
			out = blob_main
			continue
		
		lit = parse_lit(rtok, is_asm)
		word = dictionary[tok] if tok in dictionary else None
		stmt = statements[tok] if tok in statements else None
		local = ((lvars[tok] if tok in lvars else None)
			if compiling_word else None)
		
		if (lit, word, stmt, local).count(None) == 4 :
			print(tinfo.file + ", ln " + str(tinfo.line) + " " +
				(("in word \"" + word_name + "\"") if compiling_word else "at top level") +
				": unknown token \"" + tinfo.token + "\" ")
			sys.exit(1)
			raise Exception("unknown token: " + rtok + " in " +
				(("word " + word_name) if compiling_word else "main"))
		elif (lit, word, stmt, local).count(None) != 3 :
			raise Exception("collision: " + str((lit, word, stmt, local)))
		
		if lit != None :
			if type(lit) == type("") :
				if is_asm :
					out.append(lit)
				else :
					out.append(pack(fmtop0, OP_PUSH))
					data.append(("string_addr", len(out), lit))
					out.append(pack(fmt1w, 0))
					out.append(pack(fmtop1w, OP_PUSH, len(lit)))
			else :
				if is_asm :
					out.append(pack(fmt1w if lit < 0 else fmt1uw, lit))
				else :
					if o.short_lit and lit >= 0 and lit <= 0xff :
						out.append(pack(fmtop1b, OP_PUSHB, lit))
					else :
						out.append(pack(fmtop1uw, OP_PUSH, lit & word_mask))
		elif word :
			if word.is_inline :
				out.append(word.blob)
			else :
				use_word(word, dictionary, word_name if compiling_word else ": main")
				out.append(pack(fmtop1w + nfmtop0, OP_PUSH, word.adress, OP_ACALL))
		elif stmt :
			stmt(tokens, out, cstack, rstack, dictionary, static, lvars)
		elif local :
			out.append(pack(fmtop1w + nfmtop0, OP_PUSH, local.index + 1, OP_FPICK))
		else :
			raise Exception("evil error")
	else :
		if cstack :
			raise Exception("unfinished control structure")

	return (out, data, )

#-------------------------------------------------------------------------------------------------------------

def dictionary_blob(dictionary) :
	words = filter(lambda word: (not word[1].is_inline) and len(word[1].used) > 0,
		dictionary.items())
	words.sort(lambda x, y: x[1].adress - y[1].adress)
	return "".join(map(lambda w: w[1].blob, words))

#-------------------------------------------------------------------------------------------------------------

def incforth_main(tokens) :
	global dictionary_offset
	asm_words = dict(map(lambda op: (op[0].lower(),
		fth_word(op[0].lower(), True, pack(fmtop0, op[1][0]))), asm.items()))
	dictionary = {}
	dictionary.update(asm_words)
	dictionary_offset = len(pack(fmtop1w, 0, 0))
	dictionary["incforth"] = fth_word("incforth", True, None)
	
	out, data = incforth_compile(tokens, dictionary)
	dict_code = dictionary_blob(dictionary)
	jmp_to_start = (pack(fmtop1w, OP_RJMP, len(dict_code) + dictionary_offset)
		if len(dict_code) else "")
	bin = jmp_to_start + dict_code
	for lit in data :
		if lit[0] == "string_addr" :
			out[lit[1]] = pack(fmt1w, len(bin) + outp_lbl_to_adr(out, len(out)))
			out.append(lit[2])
		else :
			raise Exception("evil")
	bin += "".join(out)

	pprint.pprint(
		map(lambda w: str(w[1].adress) + "-" + str(w[1].adress + len(w[1].blob)) + ":" + w[1].name,
		ifilter(lambda w0: not w0[1].is_inline and w0[1].used, dictionary.items())))
	stats = ("code size: %iB" % len(bin), ) # staticaly allocated heap
	return (bin, stats)

#-------------------------------------------------------------------------------------------------------------

def run() :
	wd = os.getcwd()
	argp = argparse.ArgumentParser(description="Incompatible Forth Compiler")
	argp.add_argument("source", nargs="+",
			  help = "input source file")
	argp.add_argument("-c", "--cfg", dest="config",
			  default = None,
			  help = "configuration file")
	argp.add_argument("-o", "--out", dest="output",
			  default = os.path.join(wd, "xxx.bin"),
			  help = "output file name")
	argp.add_argument("-e", "--eol", dest="linesep",
			  default = "os",
			  help = "output file name")
	#endianness
	#word size
	
	args = argp.parse_args()

	environ_init(16, "little", 1024, 1024)
	#heap_size = 1024 heap_start = 1024 # offsett within vm memory pool

	global lnsep #TODO screw
	lnsep = {
		"os" : os.linesep,
		"dos" : "\r\n",
		"unix" : "\n"
	}[args.linesep.lower()]
	
	preproc_init(lnsep)

	sources = []
	for src_file in args.source :
		f = None
		try :
			f = io.open(src_file)
			sources.append(source_info(os.path.basename(src_file), f.read()))
			f.close()
			f = None
		except :
			if f :
				f.close()
			print("error reading file \"" + src_file + "\"")
			sys.exit(1)

	tokens = preproc(sources)
	out, stats = incforth_main(tokens)
	#print binascii.hexlify(out)
	
	f = None
	try :
		f = io.open(args.output, "wb")
		f.write(out)
		f.close()
	except :
		print("error while writing output file")
		sys.exit(1)

	print(stats)

	sys.exit(0)

#-------------------------------------------------------------------------------------------------------------

if __name__ == "__main__" :
	run()

#-------------------------------------------------------------------------------------------------------------

