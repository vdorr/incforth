
: sort ( a-addr cnt sz xt -- )
	0 0 false locals| done val jj comparer itemsz itemcnt A |
	1 cnt 1 - ?do
		i A + @ to val
		i 1 - to jj
		false to done
		begin
			false if
				
			else
				true to done
			then
		done until
		val A jj + 1 + ! \ XXX use move!!!
	loop
;

( utils ------------------------------------------------------------------------------------- )

\ : cmp ( a-addr1 a-addr2 -- n )  ;

: sort ( a-addr cnt sz xt -- )
	0 0 locals| done jj comparer sz cnt A |
	1 cnt 1 - ?do
		i A + here sz cmove
		i 1 - to jj
		0 to done
		begin
			A jj + here comparer execute 0> if
				A jj + 1 + A jj + sz cmove
				jj 1 - to jj
				jj 0 < if
					true to done
				then
			else
				true to done
			then
		done until
		here A jj + 1 + sz cmove
	loop
;

( create aa 5 , 3 , 1 , 9 , 8 ,
aa 5 cells dump
aa 5 1 cells :noname @ @ - ; sort
aa 5 cells dump )

