echo "#do not edit this file, it's autogenerated using mkasmdef.sh - edit that instead" > ./vm6101def.py
echo "regs={}" >> ./vm6101def.py
grep -E "^#define R_" ./vm6101.h | awk '{ print $2 "=" $3; print "regs[\"" $2 "\"]=" $2; }' >> ./vm6101def.py
echo "asm={}" >> ./vm6101def.py
echo "OPARG_NONE = 1" >> ./vm6101def.py
echo "OPARG_1WORD = 2" >> ./vm6101def.py
echo "OPARG_REG = 3" >> ./vm6101def.py
echo "OPARG_DWORD = 4" >> ./vm6101def.py
echo "OPARG_BYTE = 5" >> ./vm6101def.py
grep "^OPDEF(" ./ops6101.def | awk -F " " 'BEGIN {FS="[,(]"};{ print $2 "=" $3; print "asm[\"" $2 "\"]=(" $2 "," $5 ")"; }' >> ./vm6101def.py
