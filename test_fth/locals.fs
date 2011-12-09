
: BACKTRACE inline 0x666 sysrq ;

: nest
	0xd 0xc 0xb 0xa
	locals| a b c d |
	5 to c
	BACKTRACE
;

: tst
	locals| a0 b0 c0 d0 |
	15 b0 !
	nest
	b0 @
;

nest

