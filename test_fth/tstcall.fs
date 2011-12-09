
: btr inline 0x666 sysrq ;
: .s inline 0x55 sysrq ;

: depth2
	999 max
	locals| a b |
	btr
	a b .s 2drop
;

: depth1
	6 666 depth2
	( btr )
;

depth1

( btr )
( .s empty )
