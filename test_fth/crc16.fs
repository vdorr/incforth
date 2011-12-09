
: crc16 ( data crc -- crc' )
	dup 8 lshift swap 8 rshift .s cr or	\ crc  = (uint8_t)(crc >> 8) | (crc << 8);
	.s cr
	xor				\ crc ^= data;
	.s cr
	dup 0xff and 4 rshift xor	\ crc ^= (uint8_t)(crc & 0xff) >> 4;
	.s cr
	dup 12 lshift xor		\ crc ^= crc << 12;
	.s cr
	dup 0xff and 5 lshift xor	\ crc ^= (crc & 0xff) << 5;
	.s cr
;

49 0xffff .s cr crc16 0xffff xor .s cr


