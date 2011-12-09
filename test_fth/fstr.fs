
: crc-update ( c crc -- crc' )
	xor
;

create fstr-buf 321 chars allot

: fstr ( $fstr -- $out )
	0xffff 0 locals| ii crc len out |
	
	over + swap do
		i c@ dup [char] < =
		if
			drop
			0 to ii
			begin
				i ii + c@ dup
				[char] >  <>	
			while
				ii 1+ to ii
				crc crc-update to crc
			repeat
			drop \ drop ">"
			crc resolve \ TODO
			0xffff to crc
			666
		else
			out ( char addr -- ) c!
			out 1+ to out
			1
		then
	+loop
	\ fstr-buf out fstr-buf - ( adr len )
;

decimal

: crc16 ( data crc -- crc' )
	dup 8 lshift swap 8 rshift or	\ crc  = (uint8_t)(crc >> 8) | (crc << 8);
	xor				\ crc ^= data;
	dup 0xff and 4 rshift xor	\ crc ^= (uint8_t)(crc & 0xff) >> 4;
	dup 12 lshift xor		\ crc ^= crc << 12;
	dup 0xff and 5 lshift xor	\ crc ^= (crc & 0xff) << 5;
;

49 0xffff crc16 .s

: crc-ccitt
	dup 255 and
	swap 8 rshift
	-rot
	xor .s
	dup 4 lshift xor
	tuck
	8 lshift or
	swap dup 4 rshift
	swap 3 lshift
	xor xor 65535 and
;

: tst
	[char] 1 0xffff crc-ccitt
	\ [char] 2 swap crc-ccitt
	\ [char] 3 swap crc-ccitt
;
tst .s


: tst [char] 1 . ; tst


: crc-ccitt ( data crc -- crc' )
	dup 255 and		( data crc lo8 )
	swap 8 rshift		( data lo8 hi8 )
	-rot			( hi8 data lo8 )
	xor			\ data ^= lo8 (crc)
	dup 4 lshift xor	\ data ^= data << 4
	( hi8 data' )
	tuck
	( data' hi8 data' )
	8 lshift or		\ (uint16_t)data << 8) | hi8 (crc)
	( data' data'' )
	swap dup 4 rshift	\ (uint8_t)(data >> 4)
	( data'' data' data''' )
	swap 3 lshift		\ (uint16_t)data << 3
	xor xor 65535 and
;


: crc-ccitt ( data crc -- crc' )
	dup 0x00ff and		( d: data crc lo8 )	( r: )
	swap 8 rshift		( d: data lo8 hi8 )	( r: )
	>r			( d: data lo8 )		( r: hi8 )
	xor			\ data ^= lo8 (crc)
	dup 4 lshift xor	\ data ^= data << 4
	( d: data' ) ( r: hi8 )
	dup 8 lshift r> or	\ (uint16_t)data << 8) | hi8 (crc)
	( data' data'' )
	swap dup 4 rshift		\ (uint8_t)(data >> 4)
	( data'' data' data''' )
	swap 3 lshift \ (uint16_t)data << 3
	xor xor
;

321 value fstr-buf-len
create fstr-buf
fstr-buf-len chars allot
fstr-buf fstr-buf-len 0 fill

: fstr ( $fstr -- $out )
	over over fstr-buf 0xffff 0 locals| ii crc out in-len in |
	
	over + swap do
		i c@ dup [char] < =
		if
			drop


			1 to ii
			begin
				i ii + c@ dup
			[char] > <> while
				emit
				ii 1+ to ii
			repeat
			drop
			ii 1+
		else
			out c!
			out 1+ to out
			1
		then
	+loop
;

: test s" ai0=<ai0>;" fstr ; test
fstr-buf 10 dump



