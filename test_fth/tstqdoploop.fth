
: inline .s 0x55 sysrq ;
: inline dump 0x56 sysrq ;
: inline btr 0x666 sysrq ;
: inline . 0x57 sysrq ;

( : xxx 1281 1285 ?do i .s -1 +loop ; )

: xxx  -5 0 do  i   -1 +loop ;


xxx .s

