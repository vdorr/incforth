
require core.fs

: sig_shutdown ( -- u ) inline 1 ;
: sig_resume ( -- u ) inline 2 ;
: sig_error ( -- u ) inline 3 ;
: sig_abort ( -- u ) inline 4 ;
: sig_default ( -- u ) inline 5 ;
: sig_timer ( -- u ) inline 6 ;
: sig_event ( -- u ) inline 7 ;

: sig_install ( xt sig flags -- ) 0x100 sysrq ; \ TODO throw?
: sig_remove ( xt sig -- ) 0x101 sysrq ;
: raise ( sig -- ) inline 0x102 sysrq ;
