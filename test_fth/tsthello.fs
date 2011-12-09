
: emit inline 0x58 sysrq ; immediate

: type ( -- c-addr u )
	over + swap .s do
		i c@ emit 
	loop
;

s" hello world\n" type

bye

