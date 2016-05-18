/* Written by Pieter Kockx */
#include <stdlib.h>
#include "mem.h"
#include "cpu.h"
#include "host.h"

union memory M;

static void clone_memory(struct data_image *Data)
{
	size_t i;
	for (i=0; i < DATA ;++i)
		M.seg.data[i] = Data->byte[i];
}

static void load_program(union inst *I[], int instc)
{
	size_t i;
	for (i=0; i < instc ;++i)
		M.seg.text[i] = *I[i];
	for (i=0; i < TEXT/WORD ;++i)
		free(I[i]);
}

static void initialize_registers(void)
{
	int i;
	for (i=0; i < 32 ;++i)
		R[i] = 0;
	R[28] = (int32_t) TEXT + DATA;
	R[29] = (int32_t) pMEM - 1;
	PC = 0;
	bR = 0;
}

void boot(union inst *I[],
	  int instc,
	  int inst0,
	  struct data_image *Data,
	  char opts)
{
	initialize_registers();
	load_program(I, instc);
	clone_memory(Data);
	cpu(instc, inst0, opts);
}

static void trace_execution(void)
{
	size_t i;
	for (i=0; i < TEXT/WORD ;++i) {
		if ((int32_t) i == PC)
			printf("=> ");
		if (M.word[i] != 0)
			printf("0x%03lx 0x%08x\n", i, M.word[i]);
	}
}

static void dump_memory(void)
{
	size_t i;
	printf("\n");
	for (i=TEXT/WORD; i < pMEM/WORD ;++i) {
		if (M.word[i] != 0)
			printf("(0x%03lx) %08x %010d %s\n", i, M.word[i],
								M.word[i],
								&M.byte[i*WORD]);
	}
	printf("\n");
}

static void display_state(void)
{
	size_t i;
	printf("\n------------------------------------\n");
	printf("GENERAL REGISTERS");
	printf("\n------------------------------------\n");
	for (i=0; i < 32 ;++i) {
		if (R[i] != 0)
			printf("R[%02lu]\t%010d\t"
			       "0x%08x\n", i, R[i], R[i]);
	}
	printf("------------------------------------\n");
	printf("HI = %d | LO = %d | bR = %d", HI, LO, bR);
	printf("\n------------------------------------\n");
}

void trace(void)
{
	trace_execution();
	display_state();
	dump_memory();
}

