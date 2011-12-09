
#include "vm6101.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

//---------------------------------------------------------------------------------------------

int vm_init_tiny(vm_t* vm, uint8_t* progmem, vm_word_t progmem_size,
	vm_sysrq_f sysrq)
{
	void* pool = calloc(256, 1);
	if (!pool)
	{
		return -666;
	}
	return vm_init(vm, pool, 256, 0, 64, 128, 0, progmem, progmem_size, sysrq);
}

//---------------------------------------------------------------------------------------------

int vm_init_medium(vm_t* vm, uint8_t* progmem, vm_word_t progmem_size,
	vm_sysrq_f sysrq)
{
	void* pool = calloc(2048, 1);
	if (!pool)
	{
		return -666;
	}
	return vm_init(vm, pool, 2048, 0, 512, 1024, 0, progmem, progmem_size, sysrq);
}

//---------------------------------------------------------------------------------------------

#define VMPTR(vm, np) ((vm_word_t)(((void*)(np))-((void*)((vm)->pool))))
#define NPTR(vm, vmp) ((vmp)+(void*)(vm)->pool)

//---------------------------------------------------------------------------------------------

int vm_init(vm_t* vm, void* pool, int pool_size, int d_bottom, int r_bottom,
	int heap_bottom, vm_word_t reset_vect, uint8_t* progmem, vm_word_t progmem_size,
	vm_sysrq_f sysrq)
{
	vm->sysrq = sysrq;

	vm->progmem = progmem;
	vm->progmem_size = progmem_size;

	vm->pool = pool;

	vm->db = pool + d_bottom + sizeof(vm_word_t);
	vm->d = pool + d_bottom;

	vm->fb = pool + r_bottom - sizeof(vm_word_t);//XXX 
	vm->f = vm->fb;
	vm->r = vm->f;
	//vm->r = &(vm->f[1]);
	//XXX vm->f[-2] = 666;//VMPTR(vm, vm->r);
	//XXX vm->f[-1] = reset_vect;
	//vm->f[1] = vm->r;

	vm->heapb = pool + heap_bottom/* + sizeof(vm_word_t)*/;

	vm->ip = reset_vect;

	vm->regfile[R_F] = &vm->f;
	vm->regfile[R_R] = &vm->r;
	vm->regfile[R_DB] = &vm->db;
	vm->regfile[R_D] = &vm->d;
	vm->regfile[R_HEAPB] = &vm->heapb;
	vm->regfile[R_HEAPT] = NULL;

	vm->heap_start = vm->heapb;
	vm->heap_size = pool_size - heap_bottom;

	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_print_locals(FILE* file, vm_word_t* f, vm_word_t* r)
{
	const int words_per_line = 4;
	fprintf(file, "\n");
	vm_word_t* l = f;
	int wpl = 1;
	while (l < r)//XXX <= ?
	{
		fprintf(file, "%04x%c", *++l, (wpl++ % words_per_line) ? ' ' : '\n');
	}
	if ((wpl - 1) % words_per_line)
	{
		printf("%c", '\n');
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

/*
           |--------------------------------|
           | locals                         |
           | ...                            |
           |--------------------------------|
f[-1]      | stored f pointer               |
f[0],r[0]  | return address                 |
f[1]       | locals                         |
           | ... 

*/

int vm_backtrace(FILE* file, const vm_t* vm, const int print_locals)
{
	const int words_per_line = 4;
	fprintf(file, "--- r stack trace (most recent call first) ---");
	vm_word_t* f = vm->f;
	vm_word_t* r = vm->r;
	//r--;
	while (f > vm->fb)
	{
		fprintf(file, "\n[0x%04x] <%i>", (vm_word_t)(f[0]), (int)(r - f));
		//fprintf(file, "\n[0x%04x] <%i>", f[-1], (int)(r - f + 1));
		if (print_locals)
		{
			vm_print_locals(file, f, r);
		}
		/*if (print_locals)
		{
			fprintf(file, "\n");
			vm_word_t* l = f;
			int wpl = 1;
			while (l <= r)
			{
				fprintf(file, "%04x%c", *l++,
					(wpl++ % words_per_line) ? ' ' : '\n');
				//if (!(wpl++ % words_per_line))
				//{
				//	fprintf(file, "\n");
				//}
			}
		}*/
		/*else
		{
			fprintf(file, " %i words\n", (int)(r - f + 1));
		}*/
		
		//if ((f[-1] <= VMPTR(vm, vm->fb)))
		{
				//vm->ip = *vm->f;//XXX -1 -> 0
				vm_word_t radr = f[-1];
				//r = f - (sizeof(vm_word_t) << 1);
				r = f - sizeof(vm_word_t);
				f = NPTR(vm, radr);
		}
		
		//r = f - (sizeof(vm_word_t) << 1);
		//f = NPTR(vm, r[2]);//vm_word_t prevf = f[-2];
	}

	//main frame
	fprintf(file, "\n[] <%i>", (int)(r - f));
	if (print_locals)
	{
		vm_print_locals(file, f, r);
	}
	
	fprintf(file, "--- end of r stack trace ---\n");
	return 0;
}

//---------------------------------------------------------------------------------------------

/*int evil()
{

loop:

	opcode = next_opcode();
	void* decoded = (opcode * (lbl_op_adr - lbl_op_sub)) + lbl_op_adr;
	goto decoded;
	
	lbl_op_adr:
		goto op_adr;
	lbl_op_sub:
		goto op_sub;
	//...
	op_adr:
		//...
		goto loop;
	op_sub:
	//...

	return 666;
}*/

//---------------------------------------------------------------------------------------------

#define OPDEC_JMPTAB
//#define OPDEC_INDIR
//#define OPDEC_SWITCHCASE
//#define FINE_GRAINED_TRACE
#define IP_RANGE_CHECK
#define STACK_OPS_CHECKS
#define OP_VALID_CHECK

#define USE_CACHE
#define ICACHE_SIZE	256
#define DCACHE_SIZE	128

#define FLAT_MEMORY

//---------------------------------------------------------------------------------------------

typedef enum
{
#define OPDEF(opsym, opcode, x0, x1, x2) opsym = opcode,
#include "ops6101.def"
#undef OPDEF
	OP_INVALID = 0xff
} iset_t;


//------------------------------------------------------------------------------------------------------------

//TODO exceptions.def
#define VM_INVALID_OPCODE	0x01
#define VM_TRAP			0x02
#define VM_SYSRQ_RET_CODE	0x04
#define VM_HALTED		0x08
#define VM_ARITHMETIC_EXC	0x10
#define VM_D_OVERFLOW		0x40
#define VM_D_UNDERFLOW		0x41
#define VM_F_OVERFLOW		0x42
#define VM_F_UNDERFLOW		0x43
#define VM_R_OVERFLOW		0x44
#define VM_R_UNDERFLOW		0x45

//------------------------------------------------------------------------------------------------------------

/*int fill_icache(void* pmr_tag, uint8_t* icache)
{
	vm_t* vm = (vm_t*)pmr_tag;
	vm->pm_reader(vm->pmr_tag, icache, vm->ip, ICACHE_SIZE);//can be goto used instead of call?
	icache[ICACHE_SIZE] = OP_TRAP;
	return 0;
}*/

//------------------------------------------------------------------------------------------------------------

int vm_pm_r(void* pmr_tag, uint8_t* icache, const vm_word_t ip, const int bytes)
{
	vm_t* vm = (vm_t*)pmr_tag;
	memcpy(icache, vm->progmem + ip, bytes);
	return 0;
}

//TODO #define PMLOADER(pmr_tag,cache,addr,bytes,p_rc)

//------------------------------------------------------------------------------------------------------------

__attribute__((__noinline__))
int vm_run(vm_t* vm)
{

#define FINEGRAINEDHERE()	printf("ip=0x%04x op=0x%x\n", vm->ip, op)

#define HIGH_BIT (1<<((sizeof(vm_word_t)<<3)-1))

#define WORDSIZE sizeof(vm_word_t)

#if defined(USE_CACHE) // -----------------------------------------------------------------------------------


	vm->pm_reader = vm_pm_r;//XXX
	vm->pmr_tag = vm;//XXX


	uint8_t icache[ICACHE_SIZE + 1];
	register uint8_t* ip = NULL;// = icache - ip_offsett; -- ip[vm->ip]
	vm_word_t ic_bottom = 0;
	vm_word_t ic_top = 0;

	uint8_t dcache[DCACHE_SIZE];
	uint8_t* dp = NULL;
	vm_word_t dc_bottom = 0;
	vm_word_t dc_top = 0;


#define READPMUNCACHED(pdest,addr,sz) vm->pm_reader(vm->pmr_tag,(pdest),(addr),(sz))

#define DCACHE_FILL_FROM(addr)								\
	do {										\
		READPMUNCACHED(dcache,(addr),DCACHE_SIZE);				\
		dp=dcache-(addr);							\
		dc_bottom=(addr);							\
		dc_top=(addr)+DCACHE_SIZE;						\
	} while(0)

#define IS_IN_DCACHE(i)			(((i)>=dc_bottom)&&((i)<dc_top))
#define IS_RANGE_IN_DCACHE(i,sz)	(((i)>=dc_bottom)&&(((i)+(sz))<dc_top))

#define ICACHE_FILL_FROM(addr)								\
	do {										\
		READPMUNCACHED(icache,(addr),ICACHE_SIZE);				\
		icache[ICACHE_SIZE]=OP_CACHE;						\
		ip=icache-(addr);							\
		ic_bottom=(addr);							\
		ic_top=(addr)+ICACHE_SIZE;						\
		/*fprintf(stdout, "icrefill:ip=%x,b=%x,t=%x\n",vm->ip,ic_bottom,ic_top);fflush(stdout);*/ \
	} while(0)

#define ICACHE_FILL() ICACHE_FILL_FROM(vm->ip)

#define IS_CACHED(i)		(((i)>=ic_bottom)&&((i)<ic_top))
#define IS_CACHED_RANGE(i,sz)	(((i)>=ic_bottom)&&(((i)+(sz))<ic_top))

#define ICACHE_CHECK(i) do { if (!IS_CACHED(i)) { ICACHE_FILL(); } } while(0)
#define ICACHE_CHECK_RANGE(i,sz) do { if (!IS_CACHED_RANGE((i),(sz))) { ICACHE_FILL(); } } while(0)

#define IP_INC()	do { vm->ip++; /*ICACHE_CHECK(vm->ip);*/ } while(0)
#define IP_ADD(n)	do { vm->ip+=(n); ICACHE_CHECK(vm->ip); } while(0)
#define IP_SET(n)	do { vm->ip=(n); ICACHE_CHECK(vm->ip); } while(0)

#define READPMB_CHECKED(dest_ptr)							\
	do {										\
		/*vm->ip++;*/								\
		ICACHE_CHECK(vm->ip);							\
		*((uint8_t*)(dest_ptr)) = ip[vm->ip];					\
		IP_INC();								\
	} while(0) 

#define READPMB(dest_ptr) do { *((uint8_t*)(dest_ptr)) = ip[vm->ip]; } while(0) 

#define READPMW(dest_ptr)								\
	do {										\
		ICACHE_CHECK_RANGE(vm->ip, sizeof(vm_word_t));				\
		/*printf("yyy=%x %x %x\n",ip[vm->ip-1],ip[vm->ip],ip[vm->ip+1]);*/	\
		*((vm_word_t*)(dest_ptr))= *((vm_word_t*)&(ip[vm->ip]));		\
		vm->ip += sizeof(vm_word_t);						\
	} while(0)

#define READPMDW(dest_ptr)								\
	do {										\
		ICACHE_CHECK_RANGE(vm->ip, sizeof(vm_word_t));				\
		*((vm_dword_t*)(dest_ptr))= *((vm_dword_t*)&(ip[vm->ip]));		\
		vm->ip += sizeof(vm_word_t);						\
	} while(0)

#else // -----------------------------------------------------------------------------------------------------

#define ICACHE_FILL()	do { } while(0)

#define IP_INC()	do { vm->ip++; } while(0)
#define IP_ADD(n)	do { vm->ip += (n); } while(0)
#define IP_SET(n)	do { vm->ip = (n); } while(0)

#define PMEMCPY(dest_ptr, src_index, bytes)						\
	memcpy(dest_ptr, &(vm->progmem[src_index]), bytes)

#define READPMB(dest_ptr) do { *((uint8_t*)dest_ptr) = vm->progmem[vm->ip]; } while(0) 

#define READPMW(dest_ptr)								\
	do {										\
		memcpy(dest_ptr, &(vm->progmem[vm->ip]), sizeof(vm_word_t));		\
		vm->ip += sizeof(vm_word_t);						\
	} while(0)

#endif

// -----------------------------------------------------------------------------------------------------------

#if defined(FLAT_MEMORY)



// -----------------------------------------------------------------------------------------------------------

#define READMAINB(pdest,addr)									\
	do {											\
		if ((addr)&HIGH_BIT) {								\
			(*(pdest))=*(uint8_t*)NPTR(vm,(addr)&((vm_word_t)(~HIGH_BIT)));		\
		} else {									\
			if (!IS_IN_DCACHE((addr))) {						\
				DCACHE_FILL_FROM(addr);						\
			}									\
			READPMUNCACHED((uint8_t*)(pdest),(addr),1);				\
		}										\
	} while(0)

// -----------------------------------------------------------------------------------------------------------

#define WRITEMAINB(addr,val)									\
	do {											\
		(*(uint8_t*)(NPTR(vm,(addr)&((vm_word_t)(~HIGH_BIT)))))=((uint8_t)(val));	\
	} while(0)

// -----------------------------------------------------------------------------------------------------------
		
#define READMAINW(pdest,addr)									\
	do {											\
		if ((addr)&HIGH_BIT) {								\
			*(pdest)= *((vm_word_t*)NPTR(vm,(addr)&((vm_word_t)(~HIGH_BIT))));	\
		} else {									\
			if (!IS_RANGE_IN_DCACHE((addr),sizeof(vm_word_t))) {			\
				DCACHE_FILL_FROM(addr);						\
			}									\
			*((vm_word_t*)(pdest))= *((vm_word_t*)&(dp[(addr)]));			\
		}										\
	} while(0)

/*			if (IS_CACHED_RANGE((addr),sizeof(vm_word_t))) {			\
				*((vm_word_t*)(pdest))= *((vm_word_t*)&(dp[(addr)]));		\
			} else {								\
				READPMUNCACHED((uint8_t*)(pdest),(addr),sizeof(vm_word_t));	\
			}									\
*/

// -----------------------------------------------------------------------------------------------------------

#define WRITEMAINW(addr,val)									\
	do {											\
		/*printf("WRITEMAINW addr=%x\n", addr&((vm_word_t)(~HIGH_BIT)));*/ 		\
		(*(vm_word_t*)(NPTR(vm,(addr)&((vm_word_t)(~HIGH_BIT)))))=(val);		\
	} while (0)

// -----------------------------------------------------------------------------------------------------------




#else

#define READMAINB(pdest,addr)	do { (*(pdest))=*(uint8_t*)NPTR(vm,addr); } while (0)
#define WRITEMAINB(addr,val)	do { (*(uint8_t*)(NPTR(vm, (addr))))=((uint8_t)(val)); } while (0)
#define READMAINW(pdest,addr)	do { (*(pdest))= *(vm_word_t*)NPTR(vm,(addr)); } while (0)
#define WRITEMAINW(addr,val)	do { (*(vm_word_t*)(NPTR(vm,(addr))))=(val); } while (0)

#endif





// -----------------------------------------------------------------------------------------------------------

#define VM_EXCEPTION(e) do { printf("exception 0x%x op=0x%02x before ip=0x%04x\n", e, op, vm->ip); return e; } while(0)

#define CHECK_READABLE(vmptr)	1
#define CHECK_WRITABLE(vmptr)	1


#if defined(STACK_OPS_CHECKS)

#define DGROW()		(++(vm->d))
//#define DGROW(n)	if ((++(vm->d)) >= ) { VM_EXCEPTION(VM_D_OVERFLOW); }
#define DSHRINK()	if (vm->d < vm->db) { VM_EXCEPTION(VM_D_UNDERFLOW); } else { vm->d--; }

#define DGROW2()	do { vm->d+=2*sizeof(vm_word_t); } while(0)
#define DSHRINK2()	do { vm->d-=2*sizeof(vm_word_t); } while(0)

#define RGROW()		(++(vm->r))
//#define RSHRINK(n)	(--(vm->r))
#define RSHRINK()	if (vm->r <= vm->f) { VM_EXCEPTION(VM_R_UNDERFLOW); } else { vm->r--; }

#else

#define DGROW()		(++(vm->d))
#define DSHRINK()	(--(vm->d))

#define DGROW2()	do { vm->d+=2*sizeof(vm_word_t); } while(0)
#define DSHRINK2()	do { vm->d-=2*sizeof(vm_word_t); } while(0)

#define RGROW()		(++(vm->r))
#define RSHRINK()	(--(vm->r))

#endif

#if defined(OPDEC_JMPTAB) // ---------------------------------------------------------------------------------

#define OPDEC_SWITCH(op)	goto *case_lbls[op];
#define OPDEC_CASE(opsym)	LBL_ ##opsym :
#define OPDEC_BREAK		goto lbl_preloop;
#define OPDEC_LOOP		lbl_preloop:
#define OPDEC_DEFAULT		goto lbl_preloop; //XXX!!!

#define IS_VALID_OP(op)		((op)<(sizeof(case_lbls)/sizeof(case_lbls[0])))

	static const void *case_lbls[] =
	{
#define OPDEF(opsym, x0, x1, x2, x3) &&LBL_ ##opsym,
#include "ops6101.def"
#undef OPDEF
		NULL
	};

#elif defined(OPDEC_INDIR)  // -------------------------------------------------------------------------------

#define OPDEC_SWITCH(op)
#define OPDEC_CASE(opsym)	(void) &&LBL_ ##opsym ; LBL_ ##opsym :
#define OPDEC_BREAK		goto lbl_preloop;
#define OPDEC_LOOP		lbl_preloop:
#define OPDEC_DEFAULT		goto lbl_preloop; //XXX!!!

#define IS_VALID_OP(op)		1 //XXX!!!

#elif defined(OPDEC_SWITCHCASE)  // --------------------------------------------------------------------------

#define OPDEC_CASE(opsym)	case ( opsym ) :
#define OPDEC_BREAK		break
#define OPDEC_SWITCH(op)	switch(op)
#define OPDEC_DEFAULT		default : //XXX!!!
#define OPDEC_LOOP		//lbl_preloop:

#define IS_VALID_OP(op)		1 //XXX!!!

#endif  // ---------------------------------------------------------------------------------------------------


	ICACHE_FILL();
	

	OPDEC_LOOP
#if defined(IP_RANGE_CHECK)
	while (vm->ip < vm->progmem_size)
#else
	while (1)
#endif
	{
		vm_word_t tmp;//XXX
		int pm_offset = 0;
		uint8_t op;
		vm_word_t arg0;

		//op = vm->progmem[vm->ip - pm_offset];
		//PMEMCPY(&op, vm->ip, 1);
		READPMB(&op);

#ifdef OP_VALID_CHECK
		if (!IS_VALID_OP(op))
		{
			printf("invalid op (0x%x) at ip=0x%04x\n", op, vm->ip);
			return VM_INVALID_OPCODE;
		}
#endif

#ifdef FINE_GRAINED_TRACE
		FINEGRAINEDHERE();
#endif
		vm->ip++;
#if defined(OPDEC_INDIR)
		goto *(&&LBL0_OP_XOR + (op * (&&LBL0_OP_SUB - &&LBL0_OP_ADR)));
#define OPDEF(opsym, x0, x1, x2, x3) (void)&&LBL0_ ##opsym; LBL0_ ##opsym : goto LBL_ ##opsym ;
#include "ops6101.def"
#undef OPDEF

		OPDEC_DEFAULT //XXX ignore unknown opcodes
#endif

		OPDEC_SWITCH(op)
		{
			OPDEC_CASE(OP_PUSH)
			{
				DGROW();
				READPMW(vm->d);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_DROP)
			{
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_DUP)
			{
				DGROW();
				*vm->d = vm->d[-1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_OVER)
			{
				DGROW();
				*vm->d = vm->d[-2];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_ROT)
			{
				tmp = vm->d[-2];
				vm->d[-2] = vm->d[-1];
				vm->d[-1] = vm->d[0];
				vm->d[0] = tmp;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_SWAP)
			{
				tmp = *vm->d;
				*vm->d = vm->d[-1];
				vm->d[-1] = tmp;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_TOR)
			{
				RGROW();
				*vm->r = *vm->d;
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FROMR)
			{
				DGROW();
				*vm->d = *vm->r;
				RSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_RFETCH)
			{
				DGROW();
				*vm->d = *vm->r;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_ACALL)
			{
				vm->r[1] = VMPTR(vm, vm->f);
				vm->r[2] = vm->ip;
				vm->f = &vm->r[2];
				vm->r = vm->f;
				IP_SET(*vm->d);
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_RETURN)
			{
				if (vm->f < (vm->fb + (1 * sizeof(vm_word_t))))
				{
					VM_EXCEPTION(VM_F_UNDERFLOW);
				}
				IP_SET(*vm->f);
				vm_word_t radr = vm->f[-1];
				vm->r = vm->f - (sizeof(vm_word_t));
				vm->f = NPTR(vm, radr);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_LOOP)
			{
				READPMW(&arg0);
				int x = vm->r[0] >= vm->r[-1];
				*vm->r += *vm->d;
				DSHRINK();
				if (x == (vm->r[0] >= vm->r[-1]))
				{
					IP_ADD(arg0 - sizeof(vm_word_t) - 1);
				}
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_RJMP)
			{
				READPMW(&arg0);
				IP_ADD(arg0 - sizeof(vm_word_t) - 1);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_CRJMP)
			{
				READPMW(&arg0);
				IP_ADD(*vm->d ? 0 : (arg0 - sizeof(vm_word_t) - 1));
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FETCHW)
			{
				READMAINW(vm->d, *vm->d);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_STOREW)
			{
				WRITEMAINW(*vm->d, vm->d[-1]);
				DSHRINK();
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FETCHB)
			{
				READMAINB(vm->d, *vm->d);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_STOREB)
			{
				WRITEMAINB(*vm->d, (uint8_t)vm->d[-1]);
				DSHRINK();
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FETCHREG)
			{
				DGROW();
				READPMW(&arg0);
				vm_word_t** pp = vm->regfile[arg0];
				*vm->d = ((void*)(*pp)) - (void*)vm->pool | (HIGH_BIT);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_STOREREG)
			{
				READPMW(&arg0);
				*((vm_word_t**)vm->regfile[arg0]) = NPTR(vm, *vm->d & (~(HIGH_BIT)));
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_ADD)
			{
				DSHRINK();
				vm->d[0] += vm->d[1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_SUB)
			{
				DSHRINK();
				vm->d[0] -= vm->d[1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_CH)
			{
				DSHRINK();
				vm->d[0] = vm->d[0] > vm->d[1] ? -1 : 0;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_CL)
			{
				DSHRINK();
				vm->d[0] = vm->d[0] < vm->d[1] ? -1 : 0;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_CE)
			{
				DSHRINK();
				vm->d[0] = vm->d[0] == vm->d[1] ? ((vm_word_t)-1) : 0;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_NOT)
			{
				*vm->d = ~*vm->d;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_AND)
			{
				DSHRINK();
				vm->d[0] &= vm->d[1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_OR)
			{
				DSHRINK();
				vm->d[0] |= vm->d[1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_DIVMOD)
			{
				if (!*vm->d) { return VM_ARITHMETIC_EXC; }
				div_t x = div(vm->d[-1], vm->d[0]);
				vm->d[-1] = x.rem;
				vm->d[0] = x.quot;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_MUL)
			{
				DSHRINK();
				vm->d[0] *= vm->d[1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_XOR)
			{
				DSHRINK();
				vm->d[0] ^= vm->d[1];
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_HALT)
			{
				return VM_HALTED;
			}
			OPDEC_CASE(OP_NOP)
			{
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_SYSRQ)
			{
				if (vm->sysrq(vm))
				{
					return VM_SYSRQ_RET_CODE;
				}
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_TRAP)
			{
				/*if (!IS_CACHED(vm->ip))
				{
					vm->ip--;
					//printf("icache trap encountered @ ip=%x\n", vm->ip);fflush(stdout);
					ICACHE_FILL();
				}
				else*/
				{
					return VM_TRAP;
				}
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_LSL)
			{
				/*printf("LSL(1): %i %i \n", vm->d[-1], vm->d[0]);
				vm_dword_t tmp2 = ((vm_dword_t)*vm->d) << vm->d[-1];
				*vm->d = (vm_word_t)(tmp2 >> (sizeof(vm_word_t) << 3));
				vm->d[-1] = (vm_word_t)tmp2;
				printf("LSL(2): %i %i \n", vm->d[-1], vm->d[0]);*/
				vm->d[-1] = (vm_uword_t)((vm_uword_t)vm->d[-1] << (vm_uword_t)*vm->d);
				DSHRINK();
				printf("LSL(2): %x \n", (uint16_t)vm->d[0]);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_LSR)
			{
				/*printf("LSR(1): %i %i \n", vm->d[-1], vm->d[0]);
				vm_dword_t tmp2 = ((vm_dword_t)*vm->d) >> vm->d[-1];
				*vm->d = (vm_word_t)(tmp2 >> (sizeof(vm_word_t) << 3));
				vm->d[-1] = (vm_word_t)tmp2;
				printf("LSR(2): %i %i \n", vm->d[-1], vm->d[0]);*/
				vm->d[-1] = (vm_uword_t)((vm_uword_t)vm->d[-1] >> (vm_uword_t)*vm->d);
				DSHRINK();
				printf("LSR(2): %x \n", (uint16_t)vm->d[0]);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_CRJMPN)
			{
				READPMW(&arg0);
				IP_ADD(*vm->d ? (arg0 - sizeof(vm_word_t) - 1) : 0);
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_DPICK)
			{
				*vm->d = vm->d[-(*vm->d + 1)];//TODO check range
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FPICK)
			{
				*vm->d = vm->f[*vm->d];//TODO check range
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_MOVE)
			{
				vm_word_t adr = *vm->d;
				uint8_t tmpb;
				while (adr--)
				{
					READMAINB(&tmpb, adr);//TODO optimize
					WRITEMAINB(adr, tmpb);
				}
				
				//( c-addr1 c-addr2 u -- )
				/*if (IS_VMPGMSPACE(vmptr_src))
				{
					return -666;
				}
				else
				{
					memcpy(NPTR(vm, vm->d[-1]), NPTR(vm, vm->d[-2]), *vm->d);
				}*/
				DSHRINK();
				DSHRINK();
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_PUSHDW)
			{
				DGROW();
				READPMW(vm->d);
				DGROW();
				READPMW(vm->d);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FETCHDW)
			{
				DGROW();
				*(vm_dword_t*)&vm->d[-1] = *(vm_dword_t*)NPTR(vm, *vm->d);
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_STOREDW)
			{
				*(vm_dword_t*)NPTR(vm, *vm->d) = *(vm_dword_t*)&vm->d[-2];
				DSHRINK();
				DSHRINK();
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_FSET)
			{
				vm->f[*vm->d] = vm->d[-1];
				DSHRINK();
				DSHRINK();
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_RROT)
			{
				tmp = vm->d[-2];
				vm->d[-2] = vm->d[0];
				vm->d[0] = vm->d[-1];
				vm->d[-1] = tmp;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_PUSHB)
			{
				DGROW();
				uint8_t c;
				READPMB_CHECKED(&c);
				//printf("PUSHB: %i\n", c);
				*vm->d = (vm_word_t)c;
				//READPMB(&op);
				//vm->ip++;
				OPDEC_BREAK;
			}
			OPDEC_CASE(OP_CACHE)
			{
				ICACHE_FILL();
				OPDEC_BREAK;
			}
hell:			OPDEC_DEFAULT
			{
				return VM_INVALID_OPCODE;
			}
		}
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_place_breakpoint(vm_t* vm, vm_word_t trap_adr, uint8_t* opcode)
{
#if defined(IP_RANGE_CHECK)
	if (trap_adr >= vm->progmem_size)
	{
		return 1;
	}
#endif
	*opcode = vm->progmem[trap_adr];
	vm->progmem[trap_adr] = OP_TRAP;
	return 0;
}

//---------------------------------------------------------------------------------------------

//restore the original instruction, single-step, re-insert the trap, and continue on

int vm_recover_breakpoint(vm_t* vm, uint8_t opcode)
{
	vm_word_t trap_adr = vm->ip - 1;
	vm->progmem[trap_adr] = opcode;

#if defined(IP_RANGE_CHECK)//TODO sysrq.def

	vm_word_t progmem_size = vm->progmem_size;
	vm->progmem_size = trap_adr + 1;
	if (!vm_run(vm))
	{
		vm->progmem_size = progmem_size;
		vm->progmem[trap_adr] = OP_TRAP;
		return 0;
	}
#else
	//XXX better bit field?
	static const uint8_t len[] =
	{
#define OPARG_NONE	0
#define OPARG_1WORD	sizeof(vm_word_t)
#define OPARG_REG	sizeof(vm_word_t)
#define OPDEF(opsym, opcode, opstring, oplength, optype) oplength,
#include "ops6101.def"
#undef OPDEF
#undef OPARG_NONE
#undef OPARG_1WORD
#undef OPARG_REG
	}

	vm_word_t next_trap_adr = vm->ip + len[opcode];
	uint8_t op = vm->progmem[next_trap_adr];
	vm->progmem[next_trap_adr] = OP_TRAP;
	if (vm_run(vm) == VM_TRAP)
	{
		vm->progmem[next_trap_adr] = op;
		vm->progmem[trap_adr] = OP_TRAP;
		return 0;
	}
#endif
	return 1;
}

//---------------------------------------------------------------------------------------------

int vm_d_dump(FILE* f, vm_t* vm)
{
	vm_word_t* slot = vm->db;
	//printf("depth=%i\n", (vm->d - vm->db));// / sizeof(vm_word_t));
	fprintf(f, "<%i> ", (int)(vm->d - vm->db + 1));// / (int)sizeof(vm_word_t));
	while (slot <= vm->d)
	{
		//fprintf(f, "0x%x ", (vm_word_t)(*slot));
		fprintf(f, "%i ", (vm_word_t)(*slot));
		slot++;
	}
	//fprintf(f, " \n");
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_xxd(uint8_t* p, int size, int word_size, int words_per_line)
{
	return 0;
}

//---------------------------------------------------------------------------------------------

#define MEMORY_ACCESS_CHECK

#if defined(MEMORY_ACCESS_CHECK)
#define MCHECK(vmptr) 666  //TODO
#endif

//---------------------------------------------------------------------------------------------

#define IS_VMPGMSPACE(vmptr) 0 //TODO

//---------------------------------------------------------------------------------------------

int vm_read_memory(vm_t* vm, void* dest, vm_word_t vmptr_src, vm_word_t size)
{
	if (IS_VMPGMSPACE(vmptr_src))
	{
		return -666;
	}
	else
	{
		memcpy(dest, vm->pool + vmptr_src, size);
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_dump(FILE* file, vm_t* vm, vm_word_t vmptr, vm_word_t size)
{
#define CHARS_PER_LINE	8
	int chars_per_line = 0;
	uint8_t c;
	while (size)
	{
		if (!(chars_per_line % CHARS_PER_LINE))
		{
			fprintf(file, "%04x:", vmptr);
		}
		vm_read_memory(vm, &c, vmptr, 1);
		fprintf(file, " %02x", (int)c);
		//TODO fputc("%c", isprint(c) ? c : '-', file);
		chars_per_line++;
		if (!(chars_per_line % CHARS_PER_LINE))
		{
			fprintf(file, "\n");
		}
		vmptr++;
		size--;
	}
	fprintf(file, "\n");
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_read_d(vm_t* vm, int i, vm_word_t* pd)
{
	*pd = vm->d[i];
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_write_d(vm_t* vm, int i, vm_word_t d)
{
	vm->d[i] = d;
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_push_d(vm_t* vm, vm_word_t d)
{
	*(++vm->d) = d;
	return 0;
}

//---------------------------------------------------------------------------------------------

int vm_pop_d(vm_t* vm, vm_word_t* pd)
{
	*pd = *vm->d;
	vm->d--;
	return 0;
}

//---------------------------------------------------------------------------------------------

#if 1

//---------------------------------------------------------------------------------------------

//TODO sysrq.def
#define SYSRQ_SLEEP		0x29b
#define SYSRQ_DOUT		0x29a
#define SYSRQ_BACKTRACE		0x666
#define SYSRQ_PRINT_D		0x55
#define SYSRQ_DUMP		0x56
#define SYSRQ_DOT		0x57
#define SYSRQ_EMIT		0x58
#define SYSRQ_EMITN		0x59

#define SYSRQ_PRINT		0x5a

#define SYSRQ_OPEN		0x
#define SYSRQ_CLOSE		0x
#define SYSRQ_READ		0x
#define SYSRQ_WRITE		0x
#define SYSRQ_IOCTL		0x


int sysrq_handler(vm_t* vm)
{
	FILE* file = stdout;
	vm_word_t rq_nr;
	vm_pop_d(vm, &rq_nr);
	if (rq_nr == SYSRQ_DOUT)
	{
		vm_word_t dout_nr;
		vm_word_t value;
		vm_pop_d(vm, &dout_nr);
		vm_pop_d(vm, &value);
		fprintf(file, "sysrq/dout dout_nr=%i value=%i \n", dout_nr, value);
	}
	else if (rq_nr == SYSRQ_SLEEP)
	{
		vm_word_t ms;
		vm_pop_d(vm, &ms);
		fprintf(file, "sysrq/sleep ms=%i \n", ms);
	}
	else if (rq_nr == SYSRQ_BACKTRACE)
	{
		vm_backtrace(file, vm, 1);
	}
	else if (rq_nr == SYSRQ_PRINT_D)
	{
		vm_d_dump(file, vm);
	}
	else if (rq_nr == SYSRQ_DUMP)
	{
		vm_word_t size;
		vm_word_t vmptr;
		vm_pop_d(vm, &size);
		vm_pop_d(vm, &vmptr);
		vm_dump(file, vm, vmptr, size);
	}
	else if (rq_nr == SYSRQ_DOT)
	{
		vm_word_t x;
		if (vm_pop_d(vm, &x))
		{
			return -666;//underflow or so
		}
		fprintf(file, " %i", x);
	}
	else if (rq_nr == SYSRQ_EMIT)
	{
		vm_word_t c;
		if (vm_pop_d(vm, &c))
		{
			return -666;//underflow or so
		}
		fputc((char)c, file);
	}
	else if (rq_nr == SYSRQ_EMITN)
	{
		vm_word_t size;
		vm_word_t c;
		if (vm_pop_d(vm, &size)) { return -666; }
		while (size--)
		{
			if (vm_pop_d(vm, &c)) { return -666; }
			fputc((char)c, file);
		}
	}
	else if (rq_nr == SYSRQ_PRINT)
	{
		vm_word_t size;
		vm_word_t vmptr;
		char c;
		if (vm_pop_d(vm, &size)) { return -666; }
		if (vm_pop_d(vm, &vmptr)) { return -666; }
		vm_word_t p = vmptr;
		while (size--)
		{
			if (vm_read_memory(vm, &c, p++, 1)) { return -666; }
			fputc(c, file);
		}
	}
	else
	{
		fprintf(file, "sysrq: unknown request rq_nr=%i \n", rq_nr);
		return -1;//SYSRQ_INVALID_NR;
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

//XXX forth.py producing 16bit args
#define ARG(x)				\
	((x) & 0xff),			\
	((((x) >> 8) & 0xff))

/*#define ARG(x)				\
	((x) & 0xff),			\
	((((x) >> 8) & 0xff)),		\
	((((x) >> 16) & 0xff)),		\
	((((x) >> 24) & 0xff))*/

int main(int argc, char *argv[])
{
	uint8_t* progmem;
	int progsize = 0;

	/*if (argc == 1)
	{
		uint8_t hello [] =
		{
			OP_PUSH, ARG(1),
			OP_PUSH, ARG(2),
			OP_ADD,
		};
		progmem = hello;
		progsize = (int)sizeof(hello);
	}
	else */if (argc == 2)
	{
		FILE* f = fopen(argv[1], "rb");
		fseek(f, 0, SEEK_END);
		progsize = ftell(f);
		fseek(f, 0, SEEK_SET);
		progmem = malloc(progsize);
		fread(progmem, 1, progsize, f);
		fclose(f);
	}
	else
	{
		printf("misuse");
		return -1;
	}
	
	vm_t vm;
	int rc;

	uint16_t end = 0xff00;
	printf("%ibit %s endian\n", 4 << (int)sizeof(vm_word_t),
		((char*)&end)[0] ? "big" : "little");
	printf("%iB program memory\n", progsize);
	//printf("vm.d =%i\n", vm.d);

	
	//printf(((char*)&end)[0] ? "big\n" : "little\n");

	//rc = vm_init_tiny(&vm, progmem, progsize, sysrq_handler);
	rc = vm_init_medium(&vm, progmem, progsize, sysrq_handler);
	printf("vm_init_?=%i\n", rc);

	vm_run(&vm);

	/*printf("post mortem d dump\n");
	vm_d_dump(stdout, &vm);
	printf("\n");*/
	
	free(vm.pool);
	
	return 0;
}

//---------------------------------------------------------------------------------------------

#endif

//---------------------------------------------------------------------------------------------

