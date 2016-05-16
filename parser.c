#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "parser.h"

static void reset_extr(struct extr *X)
{
	strcpy(X->mnem, "\0");
	X->reg[0] = -1;
	X->reg[1] = -1;
	X->reg[2] = -1;
	X->val = 0;
	X->addr = NULL;
}

static char (*autofill)(struct extr *, struct extr) = NULL;

static char b(struct extr *, struct extr);
static char beqz(struct extr *, struct extr);
static char bge(struct extr *, struct extr);
static char bgt(struct extr *, struct extr);
static char ble(struct extr *, struct extr);
static char blt(struct extr *, struct extr);
static char bnez(struct extr *, struct extr);
static char la(struct extr *, struct extr);
static char li(struct extr *, struct extr);
static char move(struct extr *, struct extr);

static struct pseudoDict {
	char mnem[16];
	char (*fill)(struct extr *, struct extr);
} pseudoDict[] = {{"b", b}, {"beqz", beqz}, {"bge", bge},
		  {"bgt", bgt}, {"ble", ble}, {"blt", blt}, {"bnez", bnez},
		  {"li", li}, {"mov", move}, {"move", move}, {"la", la}};

static void ascii(char *start);
static void asciiz(char *start);
static void byte(char *start);

static struct dirDict {
	char mnem[16];
	void (*redirect)(char *start);
} dirDict[] = {{"ascii", ascii}, {"asciiz", asciiz}, {"byte", byte}};

static char last_handled[32];
static struct labels {
	char *name;
	int line;
	struct labels *next;
	struct labels *prev;
} Label0 = {0};

static void init_labels(struct labels *, int, const char *);
static int alloc_labels(struct labels *, size_t);

static int *handle_labels(int line, const char *name)
{
	struct labels *curr = &Label0;
	char iden[32];
	int len = 0;
	iden[31] = '\0';
	while (*name && !isspace(*name) && len < 31)
		iden[len++] = *name++;
	iden[len] = '\0';
	strcpy(last_handled, iden);

	while (curr->next) {
		if (!strcmp(iden, curr->name)) {
			if (curr->line < 0)
				curr->line = line;
			return &curr->line;
		}
		curr = curr->next;
	}

	if (alloc_labels(curr, 32)) {
		init_labels(curr, line, iden);
		return &curr->line;
	} else
		return NULL;
}

static char *cut_comment(char *start)
{
	char *chr;
	if ((chr = strchr(start, '#')))
		*chr = '\0';
	return start;
}

static int valid_label(const char *);

static char *cut_label(char *start, bool *label_cut, char *err)
{
	char *chr = strchr(start, ':');
	if (chr) {
		*chr = '\0';
		if (valid_label(start)
		    && handle_labels(InstC, start)) {
			*label_cut = true;
			start = chr + 1;
		} else
			*err = 5;
	}
	return start;
}

static void cut_on_comma(char *args, char *cut[], size_t nr)
{
	if (args && strlen(args) > 0) {
		char *chr;
		size_t i = 0;
		do {
			while (isspace(*args) && isspace(*++args));
			cut[i++] = args;
			if ((chr = strchr(args, ','))) {
				*chr = '\0';
				args = ++chr;
			}
		} while (chr && i < nr);
	}
}

static char is_pseudo_mnem(const char *name);
static char is_directive(char *, char *, bool *);

static char *extract_mnem(struct extr *X, char *end, bool *_l_, char *err)
{
	while (*end && isspace(*end)) end++;
	if (*end) {
		char *start = end;
		while (*end && isalpha(*++end)); // ++end for '.'
		if (isspace(*end) || *end == '\0') {
			*end++ = '\0';
			if (strlen(start) > 8) { // len for strcpy
				*err = 3;
			} else if (*start == '.') {
				if (is_directive(start+1, end, _l_)) {
					end = NULL;
				} else
					*err = 4;
			} else if (isalpha(*start) && is_mnem(start)) {
				strcpy(X->mnem, start);
			} else if (!is_pseudo_mnem(start)) {
				*err = 5;
			}
		}
	}
	return end;
}

static int valid_reg(char *);

static int extract_reg(int hit, struct extr *X, char *chr)
{
	while ((chr =  strchr(chr, '$'))) {
		int reg;
		if (hit < 3 && (reg = valid_reg(chr+1)) != -1) {
			X->reg[hit] = reg;
			hit++;
			*chr = ' ';
		} else {
			hit = 4;
			break;
		}
	}
	return hit;
}

static int is_x_hex(const char *);

static void extract_label(struct extr *X, char *str, bool *label_extract)
{
	int len;
	while (*str && !isalpha(*str)) str++;
	if ((len = valid_label(str)) && !is_x_hex(str)) {
		int *addr;
		if ((addr = handle_labels(-1, str))) {
			int i = 0;
			X->addr = addr;
			*label_extract = true;
			while (i < len) str[i++] = ' ';
		}
	}
}

static int strtol_wiped(char *, char *);

static int extract_val(struct extr *X, char *str)
{
	char *nptr, *endptr;
	int hit = 0;
	int toi = 0;

	nptr = strchr(str, 'x');
	if (nptr && is_x_hex(nptr)) {
		endptr = --nptr;
		toi = (int) strtol(nptr, &endptr, 16);
		if ((hit = strtol_wiped(nptr, endptr)))
			X->val = toi;
	} else if (str) {
		while (*str && !isdigit(*str)) str++;
		if (*str && *(str-1) == '-') str--;
		endptr = nptr = str;
		toi = (int) strtol(nptr, &endptr, 10);
		if ((hit = strtol_wiped(nptr, endptr)))
			X->val = toi;
	}
	return hit;
}

static int subst_chars(char *, char, char);

struct extr parse(char *line, bool *pseudo, bool _l_[], char *err)
{
	static struct extr Xbuff;
	struct extr X, Xzero;
	reset_extr(&X);
	reset_extr(&Xzero);
	if (!*err && autofill) {
		if (!(*autofill)(&X, Xbuff))
			*err = 7;
	} else if (!*err) {
		char *cut[4] = {0};
		int i, regc = 0, valc = 0;
		line = cut_comment(line);
		line = cut_label(line, _l_, err);
		line = extract_mnem(&X, line, _l_, err);
		if (line && *line && !*err)
			cut_on_comma(line, cut, 4);
		for (i=0; i < 4 && cut[i] ;++i) {
			int brac = 0;
			brac = subst_chars(cut[i], '(', ' ');
			if (brac != subst_chars(cut[i], ')', ' ')) {
				*err = 7;
				break;
			}
			regc = extract_reg(regc, &X, cut[i]);
			if (regc > 3) {
				*err = 8;
				break;
			}
			extract_label(&X, cut[i], _l_+1);
			valc += extract_val(&X, cut[i]);
			if (valc > 1) {
				*err = 9;
				break;
			}
		}
		if (!*err && autofill) {
			Xbuff = X;
			if (!(*autofill)(&X, Xbuff))
				*err = 10;
		}
	}
	if (!*err && autofill)
		*pseudo = true;
	if (*err) {
		autofill = NULL;
		reset_extr(&Xbuff);
		return Xzero;
	} else {
		return X;
	}
}

static int strtol_wiped(char *nptr, char *endptr)
{
	if (nptr != endptr) {
		*endptr = '\0';
		while (*nptr) *nptr++ = ' ';
		return 1;
	} else
		return 0;
}

static int unalias_reg(char *str)
{
	char *dict[32] =
			{"zero", "at", "v0", "v1",
			  "a0", "a1", "a2", "a3",
			  "t0", "t1", "t2", "t3",
			  "t4", "t5", "t6", "t7",
			  "s0", "s1", "s2", "s3",
			  "s4", "s5", "s6", "s7",
			  "t8", "t9", "k0", "k1",
			  "gp", "sp", "fp", "ra"};
	int i;
	char *ptr = str;
	for (i=0; i < 32 ;++i) {
		int len = 0;
		while (*dict[i] && (*ptr == *dict[i]))
			ptr++, dict[i]++, len++;
		if (*dict[i] == '\0') {
			int j = 0;
			while (j < len) str[j++] = ' ';
			return i;
		}
		ptr = str;
	}
	return -1;
}

static int valid_reg(char *str)
{
	int reg;
	if (isdigit(*str) && (reg = atoi(str)) >= 0 && reg < 32) {
		while (isdigit(*str)) *str++ = ' ';
	} else
		reg = unalias_reg(str);
	return reg;
}

static int subst_chars(char *chr, char src, char dst)
{
	int c = 0;
	while (chr && (chr = strchr(chr, src))) {
		*chr = dst;
		c++;
	}
	return c;
}

static int is_x_hex(const char *chr)
{
	if (*chr == 'x') {
		const char *cp = chr;
		cp++;
		return (*--chr == '0') && (isdigit(*cp) || isxdigit(*cp));
	}
	return 0;
}

static int valid_label(const char *name)
{
	int i = 1;
	if (isalpha(*name)) {
		while (*++name && !isspace(*name)) {
			if (*name == '0' &&
			     *(name+1) == 'x' && is_x_hex(name+1)) {
				return 0;
			}
			i++;
		}
	}
	return (i >= 2) && (i < 31) ? i : 0;
}

static int alloc_labels(struct labels *curr, size_t siz)
{
	void *name = curr->name = malloc(siz);
	void *next = curr->next = malloc(sizeof(struct labels));
	return (name && next);
}

static void init_labels(struct labels *curr, int line, const char *name)
{
	strcpy(curr->name, name);
	curr->line = line;
	curr->next->next = NULL;
	curr->next->prev = curr;
}

void pop_labels(bool label[])
{
	int sum = label[0] + label[1];
	if (sum) {
		struct labels *curr = &Label0;
		while (curr->next)
			curr = curr->next;
		while (curr->prev && sum-- > 0) {
			curr = curr->prev;
			free(curr->next->name);
			free(curr->next);
			curr->next = NULL;
		}
	}
}

int locate_label(const char *name)
{
	int line = 0;
	struct labels *curr = &Label0;
	while (curr) {
		if (curr->name && !strcmp(curr->name, name))
			line = curr->line;
		curr = curr->next;
	}
	return line;
}

void free_labels(void)
{
	struct labels *curr = &Label0;
	if (curr->name)
		free(curr->name);
	curr = curr->next;
	while (curr->next) {
		free(curr->name);
		curr = curr->next;
		free(curr->prev);
	}
	if (curr != &Label0)
		free(curr);
}

static char move(struct extr *X, struct extr Y)
{
	if (Y.reg[2] != -1 || Y.val)
		return 0;
	strcpy(X->mnem, "addi");
	X->reg[0] = Y.reg[0];
	X->reg[1] = Y.reg[1];
	X->reg[2] = 0;

	autofill = NULL;
	return 1;
}

static char li_small(struct extr *X, struct extr Y)
{
	if (Y.reg[1] != -1 || Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "addi");
	X->reg[0] = Y.reg[0];
	X->reg[1] = 0;
	X->val = Y.val;

	autofill = NULL;
	return 1;
}


static char li_big_ori(struct extr *X, struct extr Y)
{
	strcpy(X->mnem, "ori");
	X->reg[0] = Y.reg[0];
	X->reg[1] = Y.reg[0];
	X->val = (int16_t) Y.val;

	autofill = NULL;
	return 1;
}


static char li_big(struct extr *X, struct extr Y)
{
	if (Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "lui");
	X->reg[0] = Y.reg[0];
	X->val = (Y.val >> 16);

	autofill = li_big_ori;
	return 1;
}


static char li(struct extr *X, struct extr Y)
{
	if ((X->val >> 16) > 0) {
		return li_big(X, Y);
	} else {
		return li_small(X, Y);
	}
}

static char la_ori(struct extr *X, struct extr Y)
{
	strcpy(X->mnem, "ori");
	X->reg[0] = Y.reg[0];
	X->reg[1] = Y.reg[0];
	X->addr = Y.addr;

	autofill = NULL;
	return 1;
}

static char la(struct extr *X, struct extr Y)
{
	if (Y.reg[1] != -1 || Y.reg[2] != -1 || Y.val)
		return 0;
	strcpy(X->mnem, "lui");
	X->reg[0] = Y.reg[0];
	X->addr = Y.addr;

	autofill = la_ori;
	return 1;
}


static char bnez(struct extr *X, struct extr Y)
{
	if (Y.reg[1] != -1 || Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "bne");
	X->reg[0] = Y.reg[0];
	X->reg[1] = 0;
	X->addr = Y.addr;
	X->val = Y.val;

	autofill = NULL;
	return 1;
}

static char blt_bne(struct extr *X, struct extr Y)
{
	strcpy(X->mnem, "bne");
	X->reg[0] = 1;
	X->reg[1] = 0;
	X->addr = Y.addr;
	X->val = Y.val;

	autofill = NULL;
	return 1;
}


static char blt(struct extr *X, struct extr Y)
{
	if (Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "slt");
	X->reg[0] = 1;
	X->reg[1] = Y.reg[0];
	X->reg[2] = Y.reg[1];

	autofill = blt_bne;
	return 1;
}

static char ble_beq(struct extr *X, struct extr Y)
{
	strcpy(X->mnem, "beq");
	X->reg[0] = 1;
	X->reg[1] = 0;
	X->addr = Y.addr;

	autofill = NULL;
	return 1;
}

static char ble(struct extr *X, struct extr Y)
{
	if (Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "slt");
	X->reg[0] = 1;
	X->reg[1] = Y.reg[1];
	X->reg[2] = Y.reg[0];
	X->val = Y.val;

	autofill = ble_beq;
	return 1;
}

static char bgt_bne(struct extr *X, struct extr Y)
{
	strcpy(X->mnem, "bne");
	X->reg[0] = 1;
	X->reg[1] = 0;
	X->addr = Y.addr;

	autofill = NULL;
	return 1;
}

static char bgt(struct extr *X, struct extr Y)
{
	if (Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "slt");
	X->reg[0] = 1;
	X->reg[1] = Y.reg[1];
	X->reg[2] = Y.reg[0];
	X->val = Y.val;

	autofill = bgt_bne;
	return 1;
}

static char bge_beq(struct extr *X, struct extr Y)
{
	strcpy(X->mnem, "beq");
	X->reg[0] = 1;
	X->reg[1] = 0;
	X->addr = Y.addr;

	autofill = NULL;
	return 1;
}

static char bge(struct extr *X, struct extr Y)
{
	if (Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "slt");
	X->reg[0] = 1;
	X->reg[1] = Y.reg[0];
	X->reg[2] = Y.reg[1];
	X->val = Y.val;

	autofill = bge_beq;
	return 1;
}


static char beqz(struct extr *X, struct extr Y)
{
	if (Y.reg[1] != -1 || Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "beq");
	X->reg[0] = Y.reg[0];
	X->reg[1] = 0;
	X->addr = Y.addr;
	X->val = Y.val;

	autofill = NULL;
	return 1;
}


static char b(struct extr *X, struct extr Y)
{
	if (Y.reg[0] != -1 || Y.reg[1] != -1 || Y.reg[2] != -1)
		return 0;
	strcpy(X->mnem, "beq");
	X->reg[0] = 0;
	X->reg[1] = 0;
	X->addr = Y.addr;
	X->val = Y.val;

	autofill = NULL;
	return 1;
}


static char is_pseudo_mnem(const char *name)
{
	size_t i;
	for (i=0; i < sizeof(pseudoDict)/sizeof(struct pseudoDict) ;++i) {
		if (!strcmp(name, pseudoDict[i].mnem)) {
			autofill = pseudoDict[i].fill;
			return 1;
		}
	}
	return 0;
}

static char is_directive(char *name, char *args, bool *label_cut)
{
	if (*label_cut) {
		const size_t len = sizeof dirDict / sizeof(struct dirDict);
		const int offs_in_segm = TEXT/WORD + Data.offs;
		size_t i;
		*label_cut = false;
		for (i=0; i < len ;++i) {
			if (!strcmp(name, dirDict[i].mnem)) {
				int *match = handle_labels(-1, last_handled);
				if (match)
					*match = offs_in_segm;
				(*dirDict[i].redirect)(args);
				return 1;
			}
		}
	}
	return 0;
}

static char unescape_char(const char ch);

static void ascii(char *args)
{
	size_t off = Data.offs * WORD;
	size_t offoff = off;
	char *start = args;
	char *end;
	while ((start = strchr(start, '"'))
		 && (end = strchr(++start, '"'))) {
		*end = '\0';
		while (*start && off < DATA) {
			if (*start == '\\') {
				start++;
				*start = unescape_char(*start);
			}
			Data.byte[off++] = *start++;
		}
		start++;
	}
	Data.offs += (off - offoff)/WORD + 1;
}

static void asciiz(char *args)
{
	size_t off = Data.offs * WORD;
	size_t offoff = off;
	char *start = args;
	char *end;
	while ((start = strchr(start, '"'))
		 && (end = strchr(++start, '"'))) {
		*end = '\0';
		while (*start && off < DATA) {
			if (*start == '\\') {
				start++;
				*start = unescape_char(*start);
			}
			Data.byte[off++] = *start++;
		}
		start++;
	}
	Data.offs += (off - offoff)/WORD + 1;
}

static void byte(char *args)
{
	char *cut[DATA];
	size_t off = Data.offs * WORD;
	cut_on_comma(args, cut, DATA);
	for (; off < DATA && cut[off] ;++off) {
		char *chr;
		if ((chr = strchr(cut[off], '\'')) && strchr(chr++, '\'')) {
			if ((Data.byte[off] = *chr) == '\\')
				Data.byte[off] = unescape_char(*++chr);
		} else {
			Data.byte[off] = (char) atoi(cut[off]);
		}
	}
	Data.offs += off/WORD + 1;
}

static char unescape_char(char ch)
{
	switch(ch) {
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case '\\':
		return '\\';
	case '\'':
		return '\'';
	case '0':
		return '\0';
	default:
		return ' ';
	}
}

