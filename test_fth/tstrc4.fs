
\ [ifdef] incforth
\	require core.fs
\ [endif]

\ http://en.wikipedia.org/wiki/Forth_%28programming_language%29#A_complete_RC4_cipher_program

0 value ii
0 value jj
create S[] 256 chars allot

create KEY: 64 chars allot

: ARCFOUR  ( c -- x )
	ii 1+ dup to ii 255 and		( -- i )
	S[] +  dup C@			( -- 'S[i] S[i] ) 
	dup jj +  255 and dup to jj	( -- 'S[i] S[i] j )
	S[] +  dup C@ >R		( -- 'S[i] S[i] 'S[j] )
	over swap C!			( -- 'S[i] S[i] )
	R@ rot C!			( -- S[i] )
	R> +				( -- S[i]+S[j] )
	255 and S[] + C@		( -- c x )
	xor ;

: ARCFOUR-INIT	( key len -- )
	256 min locals| len key |
	256 0 do
		i S[] i + C!
	loop
	0 to jj

	256 0  do			( key len -- )
		key i len mod + C@
		S[] i + C@ + jj + 255 and to jj
		S[] i + dup C@ swap ( c1 addr1 )
		S[] jj + dup C@  ( c1 addr1 addr2 c2 ) rot C! C!
	loop
	0 to ii  0 to jj ;

: !KEY ( c1 c2 ... cn n—store the specified key of length n )
	dup 63 U> if abort then ( key too long <64 )
	dup KEY: ( ... cn n n key: ) C!
	KEY: + KEY:  1 +  swap 
	?do i C! -1 +loop ;

KEY: 64 0 fill
S[] 256 0 fill

0x61 0x8A 0x63 0xD2 0xFB 0x5 !KEY ( KEY: 64 dump )

KEY: 5 ARCFOUR-INIT

\ S[] 256 dump


	0xDC ARCFOUR 0xEE ARCFOUR 0x4C ARCFOUR 0xF9 ARCFOUR 0x2C ARCFOUR
	.s ( Should be: F1 38 29 C9 DE )
bye

