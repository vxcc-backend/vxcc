#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ir/ir.h"
#include "argparse/argparse.h"

#define lenof(x) (sizeof(x) / sizeof(x[0]))

static const char *const usages[] = {
    "sfmt    format an S-expression",
	"vs2h    convert vxcc IR (as S-expression) to a human-readable IR representation",
	"vs2vs   parse vxcc IR (as S-expression) and then emit it again (only useful for testing)",
	"vs2asm  optimize and codegen vxcc IR to assembly",
    NULL,
};

struct cmd_struct {
    const char *cmd;
    int (*fn) (int, const char **);
};

FILE* openfile(char const* path, char const* mode) {
    assert(path);
    assert(mode);
	if (!strcmp(path, "-")) {
		if (!strcmp(mode, "r") || !strcmp(mode, "rb")) {
			return stdin;
		} else {
			return stdout;
		}
	}
	FILE* f = fopen(path, mode);
	if (!f) {
		fprintf(stderr, "Can't open file %s\n", path);
		exit(1);
	}
	return f;
}

int
cmd_sfmt(int argc, const char **argv)
{
    const char *opath = "-";
	const char *ipath = "-";
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('i', "in",  &ipath, "input file path", NULL, 0, 0),
        OPT_STRING('o', "out", &opath, "output file path", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argc = argparse_parse(&argparse, argc, argv);

	FILE* in = openfile(ipath, "r");
	struct SNode* nd = snode_parse(in);
	if (fgetc(in) != EOF && !feof(in)) {
		fclose(in);
		exit(1);
	}
	fclose(in);
	if (!nd) {
		exit(1);
	}

	FILE* out = openfile(opath, "w");
	snode_print(nd, out);
	fclose(out);

	snode_free(nd);

    return 0;
}

int
cmd_vs2h(int argc, const char **argv)
{
    const char *opath = "-";
	const char *ipath = "-";
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('i', "in",  &ipath, "input file path", NULL, 0, 0),
        OPT_STRING('o', "out", &opath, "output file path", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argc = argparse_parse(&argparse, argc, argv);

	FILE* in = openfile(ipath, "r");
	struct SNode* nd = snode_parse(in);
	if (fgetc(in) != EOF && !feof(in)) {
		fclose(in);
		exit(1);
	}
	fclose(in);
	if (!nd) {
		exit(1);
	}

	vx_CU* cu = vx_CU_parseS(nd);
	assert(cu);

	FILE* out = openfile(opath, "w");
	for (size_t i = 0; i < cu->blocks_len; i ++) {
		vx_CUBlock bl = cu->blocks[i];
		if (bl.type == VX_CU_BLOCK_IR) {
			vx_IrBlock_dump(bl.v.ir, out, 0);
		}
	}
	fclose(out);

	snode_free(nd);

    return 0;
}

int
cmd_vs2vs(int argc, const char **argv)
{
    const char *opath = "-";
	const char *ipath = "-";
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('i', "in",  &ipath, "input file path", NULL, 0, 0),
        OPT_STRING('o', "out", &opath, "output file path", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argc = argparse_parse(&argparse, argc, argv);

	FILE* in = openfile(ipath, "r");
	struct SNode* nd = snode_parse(in);
	if (fgetc(in) != EOF && !feof(in)) {
		fclose(in);
		exit(1);
	}
	fclose(in);
	if (!nd) {
		exit(1);
	}

	vx_CU* cu = vx_CU_parseS(nd);
	assert(cu);

	FILE* out = openfile(opath, "w");
	snode_print(vx_CU_emitS(cu), out);
	fclose(out);

	snode_free(nd);

    return 0;
}

int
cmd_vs2asm(int argc, const char **argv)
{
    const char *opath = "-";
	const char *ipath = "-";
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('i', "in",  &ipath, "input file path", NULL, 0, 0),
        OPT_STRING('o', "out", &opath, "output file path", NULL, 0, 0),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argc = argparse_parse(&argparse, argc, argv);

	FILE* in = openfile(ipath, "r");
	struct SNode* nd = snode_parse(in);
	if (fgetc(in) != EOF && !feof(in)) {
		fclose(in);
		exit(1);
	}
	fclose(in);
	if (!nd) {
		exit(1);
	}

	vx_CU* cu = vx_CU_parseS(nd);
	assert(cu);

	FILE* out = openfile(opath, "w");
	int cs = vx_CU_compile(cu, NULL, NULL, out, VX_BIN_ELF, NULL, VX_CU_COMPILE_MODE_FROM_CIR);
	fclose(out);

	snode_free(nd);

    return cs;
}

static struct cmd_struct commands[] = {
    {"sfmt",  cmd_sfmt},
    {"vs2h",  cmd_vs2h},
    {"vs2vs", cmd_vs2vs},
    {"vs2asm",cmd_vs2asm},
};

int
main(int argc, const char **argv)
{
    struct argparse argparse;
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_END(),
    };
    argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);
    argc = argparse_parse(&argparse, argc, argv);
    if (argc < 1) {
        argparse_usage(&argparse);
        return -1;
    }

    /* Try to run command with args provided. */
    struct cmd_struct *cmd = NULL;
    for (int i = 0; i < lenof(commands); i++) {
        if (!strcmp(commands[i].cmd, argv[0])) {
            cmd = &commands[i];
        }
    }
    if (cmd) {
        return cmd->fn(argc, argv);
    }
    return 1;
}
