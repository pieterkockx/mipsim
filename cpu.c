/* Written by Pieter Kockx */
#include "mem.h"
#include "cpu.h"
#include "host.h"

extern union memory M;
union inst *IR;

static void advance_pc(void)
{
	PC = PC + bR + 1;
	bR = 0;
}

static void fetch(void)
{
	IR = &(M.seg.text[PC]);
}

void cpu(int instc, int inst0, char flags)
{
	unsigned int halt = 0;
	PC = inst0;
	while (PC < instc && PC < TEXT/WORD && ++halt < (1 << 31)) {
		fetch();
		decode();
		if (flags % 2) trace();
		advance_pc();
	} if (halt >= (1 << 31)) {
		printf("\nError: processor possibly in infinite loop");
		printf("(halted at cycle %u\n)", halt);
	}
	if ((flags >> 1) % 2) trace();
}

