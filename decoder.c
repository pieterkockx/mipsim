/* Written by Pieter Kockx */
#include <stddef.h>
#include "mem.h"
#include "cpu.h"
#include "host.h"

#define _DECODING
 #include "dict.h"
#undef _DECODING

extern union memory M;

inline static void noop(void)
{
}

static void (*exec_opc(uint32_t opc))(void)
{
	int i;
	for (i=0; i < DICT_OPC ;++i) {
		if (opcDict[i].opc == opc)
			return opcDict[i].impl;
	}
	return noop;
}

static void (*exec_funct(uint32_t funct))(void)
{
	int i;
	for (i=0; i < DICT_FUNCT ;++i) {
		if (functDict[i].funct == funct)
			return functDict[i].impl;
	}
	return noop;
}

static void init_reg(void);
static void init_jmp(uint32_t opc);
static void init_coproc(uint32_t opc);
static void init_imm(uint32_t opc);

void decode(void)
{
	uint32_t opc = ((*IR).to_uint >> 26);
	if (opc == 0x00)
		init_reg();
	else if ((opc >> 1) == 0x01)
		init_jmp(opc);
	else if ((opc >> 2) == 0x04)
		init_coproc(opc);
	else if (opc < 0x40)
		init_imm(opc);
	R[0] = 0;
}

static struct operands {
	int32_t *rA;
	int32_t *rB;
	int32_t *rC;
} O;
static int32_t _rALU;
static int32_t _rMUX;

static void init_reg(void)
{
	O.rA = &R[(*IR).reg.rs];
	O.rB = &R[(*IR).reg.rt];
	O.rC = &R[(*IR).reg.rd];
	_rMUX = (*IR).reg.shamt;
	(*exec_funct((*IR).reg.funct))();
}

static void init_jmp(uint32_t opc)
{
	O.rC = &_rALU;
	*O.rC = (0xfc000000 & PC) | ((*IR).jmp.target);
	(*exec_opc(opc))();
}

static void init_imm(uint32_t opc)
{
	O.rC = &_rALU;
	O.rA = &R[(*IR).imm.rs];
	O.rB = &R[(*IR).imm.rt];
	*O.rC = (int32_t) (int16_t) (*IR).imm.imm;
	(*exec_opc(opc))();
}

static void init_coproc(uint32_t opc)
{
}

static void add(void)
{
	*O.rC = *O.rA + *O.rB;
}

static void addi(void)
{
	*O.rB = *O.rA + *O.rC;
}

static void addiu(void)
{
	*O.rB = *O.rA + *O.rC;
}

static void addu(void)
{
	*O.rC = *O.rA + *O.rB;
}

static void and(void)
{
	*O.rC = *O.rA & *O.rB;
}

static void andi(void)
{
	*O.rB = *O.rA & (((unsigned) *O.rC << 16) >> 16);
}

static void beq(void)
{
	if (*O.rA == *O.rB) {
		bR = *O.rC;
	}
}

static void bgtz(void)
{
	if (*O.rA > 0) {
		bR = *O.rC;
	}
}

static void blez(void)
{
	if (*O.rA <= 0) {
		bR = *O.rC;
	}
}

static void bltz(void)
{
	if (*O.rA < 0) {
		bR = *O.rC;
	}
}

static void bne(void)
{
	if (*O.rA != *O.rB) {
		bR = *O.rC;
	}
}

static void div(void)
{
	LO = *O.rA / *O.rB;
	HI = *O.rA % *O.rB;
}

static void divu(void)
{
	LO = *O.rA / *O.rB;
	HI = *O.rA % *O.rB;
}

static void j(void)
{
	PC = *O.rC - 1;
}

static void jal(void)
{
	R[31] = PC * (int)WORD;
	PC = *O.rC - 1;
}

static void jr(void)
{
	PC = *O.rA / (int)WORD;
}

static void lb(void)
{
	*O.rB = (int32_t) M.byte[*O.rA + *O.rC];
}

static void lui(void)
{
	*O.rB = (*O.rC << 16);
}


static void lw(void)
{
	*O.rB = (int32_t) M.word[(*O.rA+*O.rC)/(int)WORD];
}

static void mfhi(void)
{
	*O.rC = HI;
}

static void mflo(void)
{
	*O.rC = LO;
}

static void mthi(void)
{
	*O.rA = HI;
}

static void mtlo(void)
{
	*O.rA = LO;
}

static void mult(void)
{
	LO = (*O.rA) * (*O.rB);
}

static void or(void)
{
	*O.rC = *O.rA | *O.rB;
}

static void ori(void)
{
	*O.rB = *O.rA | (((unsigned) *O.rC << 16) >> 16);
}

static void sb(void)
{
	M.byte[*O.rA+*O.rC] = (unsigned char) *O.rB;
}

static void sll(void)
{
	*O.rC = (*O.rB << _rMUX);
}

static void sllv(void)
{
	*O.rC = (*O.rB << *O.rA);
}

static void slt(void)
{
	if (*O.rA < *O.rB)
		*O.rC = 1;
	else
		*O.rC = 0;
}

static void slti(void)
{
	if (*O.rA < *O.rC)
		*O.rB = 1;
	else
		*O.rB = 0;
}

static void sltiu(void)
{
	if (*O.rA < *O.rC)
		*O.rB = 1;
	else
		*O.rB = 0;
}

static void sltu(void)
{
	if (*O.rA < *O.rB)
		*O.rC = 1;
	else
		*O.rC = 0;
}

static void sra(void)
{
	*O.rC = (((unsigned) *O.rB) >> _rMUX);
}

static void srl(void)
{
	*O.rC = (*O.rB >> _rMUX);
}

static void srlv(void)
{
	*O.rC = (*O.rB >> *O.rA);
}

static void sw(void)
{
	M.word[(*O.rA+*O.rC)/(int)WORD] = (uint32_t) *O.rB;
}

static void sub(void)
{
	*O.rC = *O.rA - *O.rB;
}

static void subu(void)
{
	*O.rC = *O.rA - *O.rB;
}

static void syscall(void)
{
	switch(R[2]) {
	case 1: printf("%d", R[4]);
		break;
	case 4: printf("%s", &M.byte[R[4]]);
		break;
	case 5: scanf("%d", &R[2]);
		break;
	case 8: (void) getaline((char *) &M.byte[R[4]], 128);
		break;
	case 10: exit(0);
	}
}

static void xor(void)
{
	*O.rC = *O.rA ^ *O.rB;
}

static void xori(void)
{
	*O.rB = *O.rA ^ (((unsigned) *O.rC << 16) >> 16);
}

