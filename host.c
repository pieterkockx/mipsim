/* Written by Pieter Kockx */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "parser.h"
#include "host.h"

extern union memory M;
struct data_image Data = {0};
int InstC = 0;

static void Assert(void);

int main(int argc, char *argv[])
{
	union inst *I[TEXT/WORD];
	bool labels[2] = {0};
	bool line_skipped = false;
	int opt_entry;
	char opt_flags = 0;
	size_t loc;

	Assert();
	for (loc=0; loc < TEXT/WORD ;++loc) {
		I[loc] = malloc(sizeof(WORD));
		if (I[loc])
			I[loc]->to_uint = 0;
		else
			return 1;
	}
	for (InstC=0; (size_t)InstC < TEXT/WORD; ++InstC) {
		struct extr X = {0};
		bool pseudo = false;
		char errno = 0;
		char line[128];
		char *cp = line;
		labels[0] = labels[0] && line_skipped;
		labels[1] = false;
		I[InstC]->to_uint = 0;
		if (!getaline(line, 80))
			break;
		while (isspace(*cp)) cp++;
		if (!*cp) {
			InstC--;
			continue;
		} else if(!strcmp(line, "q"))
			break;
		do {
			if (pseudo) {
				pseudo = false;
				InstC++;
			}
			X = parse(cp, &pseudo, labels, &errno);
			if (errno) {
				fprintf(stderr, "Error (%d) while parsing"
						" line %d (%s):"
						" line ignored\n",
						 errno, InstC+1,
						 X.mnem);
				pop_labels(labels);
				InstC--;
				pseudo = false;
				continue;
			} else if (*X.mnem && !encode(I[InstC], X)) {
				errno = 8;
				continue;
			} else if (!*X.mnem) {
				InstC--;
				line_skipped = true;	// label \n directive
				continue;
			} else
				line_skipped = false;
		} while (pseudo);
	}
	if (!link_labels()) {
		fprintf(stderr, "Error: dangling labels\n");
		return 1;
	}
	opt_entry = locate_label("main");
	free_labels();
	while (argc-- > 1) {
		if (!strcmp(argv[argc], "trace"))
			opt_flags += 1;
		if (!strcmp(argv[argc], "report"))
			opt_flags += 2;
	}
	printf("\n");
	boot(I, InstC, opt_entry, &Data, opt_flags);
	return 0;
}

size_t getaline(char *buff, int max)
{
	size_t len;
	printf("\n(%03d) << ", InstC);
	if (fgets(buff, max, stdin) == NULL) {
		return 0;
	} else if ((len = strlen(buff))) {
		buff[len-1] = '\0';
		return len;
	}
	return 0;
}

static void Assert(void)
{
	assert(sizeof(uint32_t) == sizeof(union inst));
	assert(sizeof(uint32_t) == sizeof(struct _immediate));
	assert(sizeof(uint32_t) == sizeof(struct _register));
	assert(sizeof(uint32_t) == sizeof(struct _jump));
}

