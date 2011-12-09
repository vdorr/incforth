
: dout ( val nr -- ) 0x29a sysrq ;
: sleep ( ms -- ) 0x29b sysrq ;

0
begin
	drop 7
	begin
		dup 1 swap dout
		300 sleep
		dup 0 swap dout
		1 - dup 0 <
	until
again
