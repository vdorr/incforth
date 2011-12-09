
\ ------------------------------------------------------------------------------------------------------------

: ai:bind ;
: ai:configure ( ch rng flt k q ll lh hyst llhndlr lwhndlr lhhndlr -- ) ;
: ai:read ;
: ai:unbind ;

: di:bind ;
: di:configure ;
: di:read ;
: di:unbind ;

: do:bind ;
: do:configure ;
: do:read ;
: do:write ;
: do:toggle ;
: do:unbind ;

: tmr: ;

: cnt: ;

: rtc:timegm ;
: rtc:stamp ;

: cron:check-task ;

: gsm:sms-create ;
: gsm:sms-add-recipient ;
: gsm:sms-write ;
: gsm:sms-send ;
: gsm:sms-cancel ;

: gsm:mail-create ;
: gsm:mail-add-recipient ;
: gsm:mail-set-subject ;
: gsm:mail-write ;
: gsm:mail-send ;
: gsm:mail-cancel ;

: pb:user-by-number ;
: pb:number ;
: pb:mail ;
: pb:pwd ;
: pb:on-ring ;
: pb:privileges ;

: log:write ( s-number s-msg -- ) ;

: db:write ;
: db:read ;
: db:reset ;

: sys:delay ;
: sys:delay-until ;

\ ------------------------------------------------------------------------------------------------------------

: example-script-db-en 667 ; immediate

: example-script
	example-script-db-en db:read
	if
		exit \ disabled
	then
	0 ai:read 666 >
	if
		0 1 do:write
	else
		0 0 do:write
	then
	1 do:toggle
	c" ai0 checked" log:write
;

: invalid-progmem-addr ; \ TODO

: example-pb-rec ( -- $number $mail $pwd privileges on-ring )
	c" +420123456789" \ or 0 0 if n/a
	c" nobody@example.com" \ or 0 0 if n/a
	c" toor" \ or 0 0 if n/a
	priv:NONE priv:R priv:W priv:XM priv:XU priv:JP or or or or or \ ???
	' example-script \ or invalid-progmem-addr
;

: R+W+XM+XU+JP ; immediate

: example-alt-pb-rec ( n n n n n -- $number $mail $pwd privileges on-ring )
	if c" +420123456789" then \ or 0 0 if n/a
	if c" nobody@example.com" then \ or 0 0 if n/a
	if c" toor" then \ or 0 0 if n/a
	if priv:NONE priv:R priv:W priv:XM priv:XU priv:JP or or or or or then \ ???
	if ' example-script then \ or invalid-progmem-addr
;

: example-task-db-en 668 ; immediate

: example-task-1-alt ( -- script-addr <bmp> )
	example-task-db-en db:read
	if
		exit \ disabled
	then
	' example-script \ or invalid-progmem-addr
	0x666		\ months
	0x666		\ days in month
	0x666		\ days in week
	0x666		\ hours
	0x666		\ minutes
; immediate

: example-task-1 ( timegm -- )
	example-task-db-en db:read
	if
		exit \ disabled
	then
	timegm-minutes pick 0x666 ??
	\ ... and and ---
	if
		associated-script \ or scripts
	then
; immediate

\ ------------------------------------------------------------------------------------------------------------

: crc-update ( c crc -- crc' )
	xor
;

create fstr-buf 320 chars allot

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
	fstr-buf out fstr-buf - ( adr len )
;

\ ------------------------------------------------------------------------------------------------------------

: main
	0 0 locals| now last-cron-check |
	begin

		rtc:stamp to now

		now last-cron-check - 60 >=
		if
			now to last-cron-check
			rtc:timegm
			example-task-1
			example-task-2
			\ task-3,4,...n
			drop drop drop drop drop \ drop timegm
		then
		
	
		
	again
;

\ ------------------------------------------------------------------------------------------------------------



