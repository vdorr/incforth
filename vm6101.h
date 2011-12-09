
#pragma once

//------------------------------------------------------------------------------------------------------------

#include <stdint.h>

//TODO #include "vmconf.h"

//------------------------------------------------------------------------------------------------------------

typedef int16_t vm_word_t;
typedef uint16_t vm_uword_t;
typedef int32_t vm_dword_t;

struct vm_struct vm_struct;

//------------------------------------------------------------------------------------------------------------

typedef int (*vm_sysrq_f)(struct vm_struct* vm);

typedef int (*vm_pm_reader_f)(void* pmr_tag, uint8_t* icache,
	const vm_word_t ip, const int bytes);

//------------------------------------------------------------------------------------------------------------

typedef struct vm_struct
{
	uint8_t* pool;
	vm_word_t ip;

//#if main_mem_pm
	uint8_t* progmem;
	int progmem_size;
//#else //pm_reader
	vm_pm_reader_f pm_reader;
	void* pmr_tag;
//#endif

	vm_sysrq_f sysrq;

	vm_word_t** regfile[7];//TODO get rid of this shit

	vm_word_t* d;
	vm_word_t* db;//for depth determination

	vm_word_t* r;
	vm_word_t* f;
	vm_word_t* fb;
	
	vm_word_t* heapb;
	vm_word_t* heap_start;
	vm_word_t heap_size;
} vm_t;

//------------------------------------------------------------------------------------------------------------

int vm_run(vm_t* vm);

//------------------------------------------------------------------------------------------------------------

#define R_F		0x00
#define R_R		0x01
#define R_RB		0x02
#define R_DB		0x03
#define R_D		0x04
#define R_HEAPB		0x05
#define R_HEAPT		0x06
//#define R_SYSRQVECT	0x06

//------------------------------------------------------------------------------------------------------------

int vm_init(vm_t* vm, void* pool, int pool_size, int d_bottom, int r_bottom,
	int locals_bottom, vm_word_t reset_vect, uint8_t* progmem, vm_word_t progmem_size,
	vm_sysrq_f sysrq);

int vm_load(vm_t* vm, void* progmem, int prog_size);

int vm_reset(vm_t* vm);

//------------------------------------------------------------------------------------------------------------

