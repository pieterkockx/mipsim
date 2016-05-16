#include <string.h>
#include <stdlib.h>
#include "mem.h"
#include "parser.h"

#define _ENCODING
 #include "dict.h"
#undef _ENCODING

static struct dangling {
	int *addr;
	int instc;
	union inst *I;
	struct dangling *next;
	struct dangling *prev;
} dangling0 = {0};

static struct opcDict lookup_opc(const char *name)
{
	struct opcDict nomatch = {0};
	int i;
	for (i=0; i < DICT_OPC ;++i) {
		if(!strcmp(opcDict[i].mnem, name))
			return opcDict[i];
	}
	return nomatch;
}

static struct functDict lookup_funct(const char *name)
{
	struct functDict nomatch = {0};
	int i;
	for (i=0; i < DICT_FUNCT ;++i) {
		if(!strcmp(functDict[i].mnem, name))
			return functDict[i];
	}
	return nomatch;
}

char is_mnem(const char *name)
{
	return (lookup_opc(name).mnem) || (lookup_funct(name).mnem);
}

static char encode_opc(union inst *, struct extr, uint32_t, int []);
static char encode_funct(union inst *, struct extr, uint32_t, int []);

char encode(union inst *I, struct extr X)
{
	struct opcDict oDict;
	struct functDict fDict;

	oDict = lookup_opc(X.mnem);
	fDict = lookup_funct(X.mnem);
	if (oDict.mnem && *oDict.mnem)
		return encode_opc(I, X, oDict.opc, oDict.args);
	else if (fDict.mnem && *fDict.mnem)
		return encode_funct(I, X, fDict.funct, fDict.args);
	return 0;
}

static char encode_regs_in_imm(union inst *, int [], int []);
static char add_dangling_label(int *, union inst *, int);

static char encode_opc(union inst *I, struct extr X, uint32_t opc, int args[])
{
	char errc = 0;
	if ((opc >> 1) == 0x01) {
		I->jmp.op = opc;
		if (X.addr && *X.addr >= 0) {
			I->jmp.target = *X.addr;
			errc = 1;
		} else if (X.addr && *X.addr < 0) {
			errc = add_dangling_label(X.addr, I, -1);
		} else {
			I->jmp.target = X.val;
			errc = 1;
		}
	} else if ((opc >> 4) == 0x01) {
		return 0;
	} else if (opc == 0x01 || (opc >> 1) < 0x04) {
		if (X.addr && *X.addr >= 0) {
			int16_t offs = *X.addr - InstC - 1;
			I->imm.op = opc;
			I->imm.imm = offs;
			errc = encode_regs_in_imm(I, X.reg, args);
		} else if (X.addr && *X.addr < 0) {
			I->imm.op = opc;
			errc = encode_regs_in_imm(I, X.reg, args)
			 && add_dangling_label(X.addr, I, InstC + 1);
		}
	} else if (opc < 0x40) {
		if (X.addr && *X.addr >= 0) {
			int16_t addr = *X.addr;
			I->imm.op = opc;
			if ((opc == 0x0f))
				I->imm.imm = (addr >> 14);
			else
				I->imm.imm = (addr << 2);
			errc = encode_regs_in_imm(I, X.reg, args);
		} else if (X.addr && *X.addr < 0) {
			I->imm.op = opc;
			errc = encode_regs_in_imm(I, X.reg, args)
			 && add_dangling_label(X.addr, I, -1);
		} else {
			int16_t val = X.val;
			I->imm.op = opc;
			I->imm.imm = val;
			errc = encode_regs_in_imm(I, X.reg, args);
		}
	}
	return errc;
}

static char encode_reg(union inst *, int[], uint32_t, int []);

static char encode_funct(union inst *I, struct extr X, uint32_t funct, int args[])
{
	I->reg.op = 0x0;
	I->reg.funct = funct;
	return encode_reg(I, X.reg, X.val, args);

}

static char encode_reg(union inst *I, int reg[], uint32_t val, int args[])
{
	int i, j=0;
	for (i=0; i < 3 && args[i] ;++i) {
		switch (args[i]) {
		case RS:
			if (reg[j] >= 0) {
				I->reg.rs = reg[j++];
				continue;
			} else
				return 0;
		case RT:
			if (reg[j] >= 0) {
				I->reg.rt = reg[j++];
				continue;
			 } else
				return 0;
		case RD:
			if (reg[j] >= 0) {
				I->reg.rd = reg[j++];
				continue;
			} else
				return 0;
		case SHAMT:
			if (val < (1 << SHAMT)) {
				I->reg.shamt = val;
				continue;
			} else
				return 0;
		case NIL:
			if (reg[j] > 0)
				return 0;
			else
				continue;
		}
	}
	return 1;
}

static char encode_regs_in_imm(union inst *I, int reg[], int args[])
{
	int i, j=0;
	for (i=0; i < 3 && args[i] ;++i) {
		switch (args[i]) {
		case RS:
			if (j < 3 && reg[j] >= 0) {
				I->reg.rs = reg[j++];
				continue;
			} else
				return 0;
		case RT:
			if (j < 3 && reg[j] >= 0) {
				I->reg.rt = reg[j++];
				continue;
			} else
				return 0;
		case IMM:
			continue;
		case NIL:
			if (j < 3 && reg[j] >= 0) {
				return 0;
			}
		}
	}
	return 1;
}

static char add_dangling_label(int *addr, union inst *I, int instc)
{
	struct dangling *curr = &dangling0;

	while (curr->next) curr = curr->next;
	curr->addr = addr;
	curr->instc = instc;
	curr->I = I;
	if ((curr->next = malloc(sizeof(struct dangling)))) {
		curr->next->next = NULL;
		curr->next->prev = curr;
		return 1;
	} else
		return 0;
}

bool link_labels(void)
{
	struct dangling *curr = &dangling0;

	while (curr->next) {
		if (*curr->addr == -1) {
			return false;
		} else if (*curr->addr > TEXT) {
			int16_t addr = (*curr->addr);
			curr->I->imm.imm = addr;
		} else if (curr->instc >= 0) {
			int16_t offs = *curr->addr - curr->instc;
			curr->I->imm.imm = offs;
		} else if (curr->instc == -1) {
			if ((curr->I->jmp.op >> 1) == 0x01) {
				curr->I->jmp.target = *curr->addr;
			} else if (curr->I->imm.op == 0x0f) {
				int16_t addr = (*curr->addr >> 14);
				curr->I->imm.imm = addr;
			} else {
				int16_t addr = *curr->addr;
				curr->I->imm.imm = (addr << 2);
			}
		}
		curr = curr->next;
	}
	while (curr->prev) {
		curr = curr->prev;
		free(curr->next);
	}
	return true;
}

