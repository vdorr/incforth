
\ BIG FAT TODO: make this file word size and endiann independent

( registers --------------------------------------------------------------------------------- )

: R_HEAPB asm inline 0x05 ;

: R_F asm inline 0x00 ;

: R_R asm inline 0x01 ;
: R_RB asm inline 0x02 ;
: R_DB asm inline 0x03 ;
: R_D asm inline 0x04 ;
( : R_HEAPT asm inline 0x06 ; )

( alu --------------------------------------------------------------------------------------- )

: + inline OP_ADD ;
: - inline OP_SUB ;
: * inline OP_MUL ;
: / inline OP_DIVMOD OP_SWAP OP_DROP ;
: mod inline OP_DIVMOD OP_DROP ;
: /mod inline OP_DIVMOD ;

: lshift ( x1 u -- x2 ) asm inline OP_LSL ;
: rshift ( x1 u -- x2 ) asm inline OP_LSR ;

: and inline OP_AND ; ( bit-by-bit logical and )
: or inline OP_OR ;
: not inline OP_NOT ;
: xor ( x1 x2 -- x3 ) inline OP_XOR ;

: abs asm inline  ( n -- u )
	OP_DUP
	OP_PUSH 0
	OP_CL
	OP_CRJMP 8
	OP_PUSH 0
	OP_SWAP
	OP_SUB ;

: = inline OP_CE ;
: < inline OP_CL ;
: > inline OP_CH ;
: 0> inline 0 > ;
: <> ;
: u< ( u1 u2 -- flag ) inline OP_CL ;
: u> ( u1 u2 -- flag ) inline OP_CH ;
: 0< inline 0 < ;
: 0= inline 0 = ;
: 0<> inline 0 = not ;

: */ ( n1 n2 n3 -- n4 ) inline ; ( TODO )
: */mod ( n1 n2 n3 -- n4 n5 ) inline ; ( TODO )

: 1+ ( n -- n+1 ) inline 1 + ;
: 1- ( n -- n-1 ) inline 1 - ;

( stack operators --------------------------------------------------------------------------- )

: drop inline OP_DROP ;
: swap ( x1 x2 -- x2 x1 ) inline OP_SWAP ;
: dup inline OP_DUP ;
: over ( x1 x2 -- x1 x2 x1 ) inline OP_OVER ;
: rot ( x1 x2 x3 -- x2 x3 x1 ) inline OP_ROT ;
: nip ( x1 x2 -- x2 ) inline OP_SWAP OP_DROP ;
: tuck ( x1 x2 -- x2 x1 x2 ) inline OP_SWAP OP_OVER ;
: -rot ( x1 x2 x3 -- x3 x1 x2 ) inline OP_RROT ;

: ?dup ( x -- 0 | x x ) asm inline OP_DUP OP_CRJMP 4 OP_DUP ;

: 2drop ( x1 x2 -- ) inline OP_DROP OP_DROP ;
: 2swap ( x1 x2 x3 x4 -- x3 x4 x1 x2 ) inline ; ( TODO )
: 2dup ( x1 x2 -- x1 x2 x1 x2 ) inline OP_OVER OP_OVER ;
: 2over ( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 ) inline ; ( TODO )

: >R ( x -- ) ( R:  -- x ) inline OP_TOR ;
: R> ( -- x ) ( R:  x -- ) inline OP_FROMR ;
: R@ ( -- x ) ( R:  x -- x ) inline OP_FROMR OP_DUP OP_TOR ;

: 2>R ( x1 x2 -- ) ( R:  -- x1 x2 ) inline OP_SWAP OP_TOR OP_TOR ;
: 2R> ( -- x1 x2 ) ( R:  x1 x2 -- ) inline OP_FROMR OP_FROMR OP_SWAP ;
: 2R@ ( -- x1 x2 ) ( R:  x1 x2 -- x1 x2 )
	inline OP_FROMR OP_FROMR OP_OVER OP_OVER OP_TOR OP_TOR OP_SWAP ;

: depth ( -- +n ) ; \ TODO

( memory ------------------------------------------------------------------------------------ )

: ! ( x a-addr -- ) inline OP_STOREW ;
: @ ( a-addr -- x ) inline OP_FETCHW ;
: C! ( char c-addr -- ) inline OP_STOREB ;
: C@ ( c-addr -- char ) inline OP_FETCHB ;

: align ( -- ) inline ;
: aligned ( addr -- a-addr ) ;
: chars ( n1 -- n2 ) inline ; ( n2 is the size in address units of n1 characters. )
: cells ( n1 -- n2 ) inline 2 * ; ( n2 is the size in address units of n1 cells. )

: cmove ( addr1 addr2 u -- ) asm inline OP_MOVE ;
: move ( c-addr1 c-addr2 u -- ) asm inline cells OP_MOVE ;

: fill ( c-addr u char -- ) swap rot swap over + swap do dup i c! loop drop ;

: here ( -- addr ) asm inline OP_FETCHREG R_HEAPB ; ( addr is the data-space pointer)
: unused ( -- u ) inline ( XXX!!! ) ;

: allot ( n -- )  OP_FETCHREG R_HEAPB OP_ADD OP_STOREREG R_HEAPB ;

: , ( x -- ) here ! 1 cells here + OP_STOREREG R_HEAPB ;

: sp@ ( -- addr ) asm inline OP_FETCHREG R_D ;
: sp! ( addr -- ) asm inline OP_STOREREG R_D ;
: rp@ ( -- addr ) asm inline OP_FETCHREG R_R ;
: rp! ( addr -- ) asm inline OP_STOREREG R_R ;
: ip@ ( -- addr ) asm OP_FETCHREG R_D ; immediate

( etc --------------------------------------------------------------------------------------- )

: execute ( i*x xt -- j*x ) asm inline OP_ACALL ;

: exit asm inline OP_RETURN ;

: min ( n1 n2 -- n3 ) asm OP_OVER OP_OVER OP_CH OP_CRJMP 4 OP_SWAP OP_DROP ;
: max ( n1 n2 -- n3 ) asm OP_OVER OP_OVER OP_CL OP_CRJMP 4 OP_SWAP OP_DROP ;
: within ( test low high -- flag ) OP_OVER OP_SUB OP_TOR OP_SUB OP_FROMR OP_CL ;

: true ( -- true ) inline -1 ;
: false ( -- false ) inline 0 ;

: bye asm inline OP_HALT ;

: abort ( -- ) inline OP_HALT ;
( ABORT" )

: sysrq ( ... args vect -- results ... ) inline OP_SYSRQ ;

: emit inline 0x58 sysrq ;

: type ( c-addr u -- )
	over + swap do
		i c@ emit 
	loop
;

: .s ( -- ) inline 0x55 sysrq ;
: dump inline 0x56 sysrq ;
: btr inline 0x666 sysrq ;
: . inline 0x57 sysrq ;
\ : cr inline 0x0a 0x58 sysrq ;
: cr inline [char] \n emit ;
: ? ( a-addr -- ) @ . ;

: ok inline ;

( ------------------------------------------------------------------------------------------- )

