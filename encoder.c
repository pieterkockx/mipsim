/* Written by Pieter Kockx */
#include <string.h>
#include <stdlib.h>

#include "mem.h"
#include "parser.h"

#define _ENCODING
 #include "dict.h"
#undef _ENCODING

static struct labels_two {
	int *def;
	int ref;
	union inst *I;
	struct labels_two *next;
	struct labels_two *prev;
} label0 = {0};

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

bool is_mnem(const char *name)
{
	return (lookup_opc(name).mnem) || (lookup_funct(name).mnem);
}

static bool encode_opc(union inst *, struct extr, uint32_t, int []);
static bool encode_funct(union inst *, struct extr, uint32_t, int []);

bool encode(union inst *I, struct extr X)
{
	struct opcDict oDict;
	struct functDict fDict;

	oDict = lookup_opc(X.mnem);
	fDict = lookup_funct(X.mnem);
	if (oDict.mnem && *oDict.mnem)
		return encode_opc(I, X, oDict.opc, oDict.args);
	else if (fDict.mnem && *fDict.mnem)
		return encode_funct(I, X, fDict.funct, fDict.args);
	return false;
}

static bool encode_regs_in_imm(union inst *, int [], int []);
static bool add_forward_ref(int *, union inst *, int);

static bool encode_opc(union inst *I, struct extr X, uint32_t opc, int args[])
{
	bool success = false;
	if ((opc >> 1) == 0x01) {
		I->jmp.op = opc;
		if (X.addr && *X.addr >= 0) {
			I->jmp.target = *X.addr;
			success = true;
		} else if (X.addr && *X.addr < 0) {
			success = add_forward_ref(X.addr, I, -1);
		} else {
			I->jmp.target = X.val;
			success = true;
		}
	} else if ((opc >> 4) == 0x01) {
		return 0;
	} else if (opc == 0x01 || (opc >> 1) < 0x04) {
		if (X.addr && *X.addr >= 0) {
			int16_t offs = *X.addr - InstC - 1;
			I->imm.op = opc;
			I->imm.imm = offs;
			success = encode_regs_in_imm(I, X.reg, args);
		} else if (X.addr && *X.addr < 0) {
			I->imm.op = opc;
			success = encode_regs_in_imm(I, X.reg, args)
			 && add_forward_ref(X.addr, I, InstC + 1);
		}
	} else if (opc < 0x40) {
		if (X.addr && *X.addr >= 0) {
			int16_t addr = *X.addr;
			I->imm.op = opc;
			if ((opc == 0x0f))
				I->imm.imm = (addr >> 14);
			else
				I->imm.imm = (addr << 2);
			success = encode_regs_in_imm(I, X.reg, args);
		} else if (X.addr && *X.addr < 0) {
			I->imm.op = opc;
			success = encode_regs_in_imm(I, X.reg, args)
			 && add_forward_ref(X.addr, I, -1);
		} else {
			int16_t val = X.val;
			I->imm.op = opc;
			I->imm.imm = val;
			success = encode_regs_in_imm(I, X.reg, args);
		}
	}
	return success;
}

static bool encode_reg(union inst *, int[], uint32_t, int []);

static bool encode_funct(union inst *I, struct extr X, uint32_t funct, int args[])
{
	I->reg.op = 0x0;
	I->reg.funct = funct;
	return encode_reg(I, X.reg, X.val, args);

}

static bool encode_reg(union inst *I, int reg[], uint32_t val, int args[])
{
	int i, j=0;
	for (i=0; i < 3 && args[i] ;++i) {
		switch (args[i]) {
		case RS:
			if (reg[j] >= 0) {
				I->reg.rs = reg[j++];
				continue;
			} else
				return false;
		case RT:
			if (reg[j] >= 0) {
				I->reg.rt = reg[j++];
				continue;
			 } else
				return false;
		case RD:
			if (reg[j] >= 0) {
				I->reg.rd = reg[j++];
				continue;
			} else
				return false;
		case SHAMT:
			if (val < (1 << SHAMT)) {
				I->reg.shamt = val;
				continue;
			} else
				return false;
		case NIL:
			if (reg[j] > 0)
				return false;
			else
				continue;
		}
	}
	return true;
}

static bool encode_regs_in_imm(union inst *I, int reg[], int args[])
{
	int i, j=0;
	for (i=0; i < 3 && args[i] ;++i) {
		switch (args[i]) {
		case RS:
			if (j < 3 && reg[j] >= 0) {
				I->reg.rs = reg[j++];
				continue;
			} else
				return false;
		case RT:
			if (j < 3 && reg[j] >= 0) {
				I->reg.rt = reg[j++];
				continue;
			} else
				return false;
		case IMM:
			continue;
		case NIL:
			if (j < 3 && reg[j] >= 0) {
				return false;
			}
		}
	}
	return true;
}

static bool add_forward_ref(int *def, union inst *I, int ref)
{
	struct labels_two *curr = &label0;

	while (curr->next) curr = curr->next;
	curr->def = def;
	curr->ref = ref;
	curr->I = I;
	if ((curr->next = malloc(sizeof(struct labels_two)))) {
		curr->next->next = NULL;
		curr->next->prev = curr;
		return true;
	} else
		return false;
}

bool link_labels(void)
{
	struct labels_two *curr;
	for (curr=&label0; curr->next; curr=curr->next) {
		if (*curr->def == -1) {
			return false;
		} else if (*curr->def > TEXT) {
			int16_t def = (*curr->def);
			curr->I->imm.imm = def;
		} else if (curr->ref >= 0) {
			int16_t offs = *curr->def - curr->ref;
			curr->I->imm.imm = offs;
		} else if (curr->ref == -1) {
			if ((curr->I->jmp.op >> 1) == 0x01) {
				curr->I->jmp.target = *curr->def;
			} else if (curr->I->imm.op == 0x0f) {
				int16_t def = (*curr->def >> 14);
				curr->I->imm.imm = def;
			} else {
				int16_t def = *curr->def;
				curr->I->imm.imm = (def << 2);
			}
		}
	}
	while (curr->prev) {
		curr = curr->prev;
		free(curr->next);
	}
	return true;
}

