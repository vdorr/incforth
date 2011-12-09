
: dout ( val nr -- ) 0x29a sysrq ;
: sleep ( ms -- ) 0x29b sysrq ;

begin
	7 0  do
		1 i dout
		300 sleep
		0 i dout
	loop
again
