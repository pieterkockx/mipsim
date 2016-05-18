/* Written by Pieter Kockx */
#include <glob.h>
#include <stdint.h>

struct _immediate {
	uint32_t imm:16;
	uint32_t rt:5;
	uint32_t rs:5;
	uint32_t op:6;
};

struct _register {
	uint32_t funct:6;
	uint32_t shamt:5;
	uint32_t rd:5;
	uint32_t rt:5;
	uint32_t rs:5;
	uint32_t op:6;
};

struct _jump {
	uint32_t target:26;
	uint32_t op:6;
};

union inst {
	struct _immediate imm;
	struct _register  reg;
	struct _jump      jmp;

	uint32_t to_uint;
};

#define WORD   sizeof(union inst)
#define TEXT    256 * WORD
#define DATA    256 * WORD
#define HEAP    256 * WORD
#define pMEM   1024 * WORD

struct _segment {
	union    inst text[TEXT/WORD];
	unsigned char data[DATA];
	unsigned char heap[HEAP];
	unsigned char stack[pMEM - HEAP - DATA - TEXT];
};

union memory {
	struct _segment seg;
	unsigned char byte[pMEM];
	uint32_t word[pMEM/WORD];
};

extern struct data_image {
	size_t offs;
	unsigned char byte[DATA];
} Data;

