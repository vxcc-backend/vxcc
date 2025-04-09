#include <time.h>
#include "../ir/ir.h"
#include "../ir/passes.h"
#include "sexpr.h"

vx_IrVar vx_IrVar_parseS(vx_CU* cu, struct SNode* nd)
{
	snode_expect(nd, S_INTEGER);
	vx_IrVar out;
	sscanf(nd->value, "%zu", &out);
	return out;
}

vx_IrTypedVar vx_IrTypedVar_parseS(vx_CU* cu, struct SNode* nd)
{
	nd = snode_expect(nd, S_LIST)->list;

	struct SNode* s0 = snode_geti_expect(nd, 0);
	snode_expect(s0, S_STRING);
	vx_IrType* ty = vx_CU_typeByName(cu, s0->value);
	if (!ty) {
		fprintf(stderr, "type %s not registered\n", s0->value);
		exit(1);
	}

	vx_IrVar var = vx_IrVar_parseS(cu, snode_geti_expect(nd, 1));

	return (vx_IrTypedVar) {
		.type = ty,
		.var = var,
	};
}

vx_IrBlock* vx_IrBlock_parseS(vx_CU* cu, struct SNode* nd);

vx_IrValue vx_IrValue_parseS(vx_CU* cu, struct SNode* nd)
{
	nd = snode_expect(nd, S_LIST)->list;

	struct SNode* kind = snode_geti_expect(nd, 0);
	snode_expect(kind, S_SYMBOL);
    assert(kind->value);

	if (!strcmp(kind->value, "int")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_INTEGER);
		long long value;
		sscanf(v->value, "%lli", &value);
		return VX_IR_VALUE_IMM_INT(value);
	}
	else if (!strcmp(kind->value, "float")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_FLOAT);
		double value;
		sscanf(v->value, "%lf", &value);
		return VX_IR_VALUE_IMM_FLT(value);
	}
	else if (!strcmp(kind->value, "var")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		return VX_IR_VALUE_VAR(vx_IrVar_parseS(cu, v));
	}
	else if (!strcmp(kind->value, "uninit")) {
		return VX_IR_VALUE_UNINIT();
	}
	else if (!strcmp(kind->value, "label")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_INTEGER);
		size_t value;
		sscanf(v->value, "%zu", &value);
		return VX_IR_VALUE_ID(value);
	}
	else if (!strcmp(kind->value, "x86-cc")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_STRING);
		return VX_IR_VALUE_X86_CC(v->value);
	}
	else if (!strcmp(kind->value, "type")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_STRING);
		vx_IrType* ty = vx_CU_typeByName(cu, v->value);
		if (!ty) {
			fprintf(stderr, "type %s not registered\n", v->value);
			exit(1);
		}
		return VX_IR_VALUE_TYPE(ty);
	}
	else if (!strcmp(kind->value, "blockref")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_STRING);
		vx_IrBlock* b = vx_CU_blockByName(cu, v->value);
		if (!b) {
			fprintf(stderr, "named block %s not registered\n", v->value);
			exit(1);
		}
		return VX_IR_VALUE_BLKREF(b);
	}
	else if (!strcmp(kind->value, "symref")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		snode_expect(v, S_STRING);
		return VX_IR_VALUE_SYMREF(faststrdup(v->value));
	}
	else if (!strcmp(kind->value, "block")) {
		struct SNode* v = snode_geti_expect(nd, 1);
		return VX_IR_VALUE_BLK(vx_IrBlock_parseS(cu, v));
	}
	else {
		fprintf(stderr, "invalid value type\n");
		exit(1);
	}
}

vx_IrOp* vx_IrOp_parseS(vx_CU* cu, struct SNode* s)
{
	s = snode_expect(s, S_LIST)->list;

	struct SNode* outs = snode_expect(snode_kv_get_expect(s, "outs"), S_LIST)->list;
	struct SNode* params = snode_expect(snode_kv_get_expect(s, "params"), S_LIST)->list;
	struct SNode* args = snode_expect(snode_kv_get_expect(s, "args"), S_LIST)->list;
	char const * name = snode_expect(snode_kv_get_expect(s, "name"), S_STRING)->value;

	vx_IrOpType opty;
	if (!vx_IrOpType_parsec(&opty, name)) {
		fprintf(stderr, "there is no operation with the name \"%s\"\n", name);
		exit(1);
	}

	vx_IrOp* op = fastalloc(sizeof(vx_IrOp));
	vx_IrOp_init(op, opty, NULL);

	for (; outs; outs = outs->next) {
		vx_IrTypedVar out = vx_IrTypedVar_parseS(cu, outs);
		vx_IrOp_addOut(op, out.var, out.type);
	}

	for (; params; params = params->next) {
		struct SNode* pair = snode_expect(params, S_LIST)->list;
		vx_IrName pair_name = vx_IrName_parsec(snode_expect(snode_geti_expect(pair, 0), S_STRING)->value);
		vx_IrValue pair_value = vx_IrValue_parseS(cu, snode_geti_expect(pair, 1));
		vx_IrOp_addParam_s(op, pair_name, pair_value);
	}

	for (; args; args = args->next) {
		vx_IrValue v = vx_IrValue_parseS(cu, args);
		vx_IrOp_addArg(op, v);
	}

	return op;
}

vx_IrBlock* vx_IrBlock_parseS(vx_CU* cu, struct SNode* s)
{
	s = snode_expect(s, S_LIST)->list;

	struct SNode* args = snode_expect(snode_kv_get_expect(s, "args"), S_LIST)->list;
	struct SNode* ops = snode_expect(snode_kv_get_expect(s, "ops"), S_LIST)->list;
	struct SNode* rets = snode_expect(snode_kv_get_expect(s, "rets"), S_LIST)->list;

	vx_IrBlock* block = vx_IrBlock_initHeap(NULL, NULL);
    block->is_root = false;

	for (; args; args = args->next) {
		vx_IrTypedVar arg = vx_IrTypedVar_parseS(cu, args);
		vx_IrBlock_addIn(block, arg.var, arg.type);
	}

	for (; ops; ops = ops->next) {
		vx_IrOp* op = vx_IrOp_parseS(cu, ops);
		op->parent = block;
		vx_IrBlock_addOp(block, op);

        FOR_INPUTS(op, inp, ({
            if (inp.type == VX_IR_VAL_BLOCK) {
                inp.block->parent = block;
            }
        }));
	}

	for (; rets; rets = rets->next) {
		vx_IrVar ret = vx_IrVar_parseS(cu, rets);
		vx_IrBlock_addOut(block, ret);
	}

	return block;
}

bool vx_bool_parseS(vx_CU* cu, struct SNode* s)
{
	snode_expect(s, S_INTEGER);
	char const* v = s->value;

	char first = v[0];
	if (first && !v[1]) {
		if (first == '0')
			return false;
		if (first == '1')
			return true;
	}

	fprintf(stderr, "booleans are either `0` or `1`\n");
	exit(1);
}

void vx_CUBlock_parseS(vx_CU* cu, struct SNode* s)
{
	s = snode_expect(s, S_LIST)->list;

	struct SNode* attribs = snode_expect(snode_geti_expect(s, 0), S_LIST)->list;
	struct SNode* inner = snode_expect(snode_geti_expect(s, 1), S_LIST)->list;

	// TODO: other block types
	char const* kindval = snode_expect(inner, S_SYMBOL)->value;
    assert(kindval);
    if (strcmp(kindval, "block")) {
		fprintf(stderr, "that's not how cu-block works (1)\n");
		exit(1);
	}
	inner = inner->next;

	vx_IrBlock* block = vx_IrBlock_parseS(cu, inner);
	block->is_root = true;
	vx_CIrBlock_fix(cu, block);

	char const* name = snode_expect(snode_kv_get_expect(attribs, "name"), S_STRING)->value;
	block->name = name;

	bool export = vx_bool_parseS(cu, snode_kv_get_expect(attribs, "export"));

	vx_CUBlock* cb = vx_CU_addBlock(cu);
	cb->do_export = export;
	cb->type = VX_CU_BLOCK_IR;
	cb->v.ir = block;
}

vx_OptConfig vx_OptConfig_parseS(vx_CU* cu, struct SNode* s)
{
	s = snode_expect(s, S_LIST)->list;

	vx_OptConfig res;
	res.max_total_cmov_inline_cost = atoi(snode_expect(snode_kv_get_expect(s, "max_total_cmov_inline_cost"), S_INTEGER)->value);
	res.consteval_iterations = atoi(snode_expect(snode_kv_get_expect(s, "consteval_iterations"), S_INTEGER)->value);
    res.loop_simplify = vx_bool_parseS(cu, snode_kv_get_expect(s, "loop_simplify"));
    res.if_eval = vx_bool_parseS(cu, snode_kv_get_expect(s, "if_eval"));
	return res;
}

/** also adds type to cu */
vx_IrType* vx_IrType_parseS(vx_CU* cu, struct SNode* s)
{
	s = snode_expect(s, S_LIST)->list;

	char const* name = snode_expect(snode_geti_expect(s, 0), S_STRING)->value;
	char const* kind = snode_expect(snode_geti_expect(s, 1), S_SYMBOL)->value;
	struct SNode* inner = snode_expect(snode_geti_expect(s, 2), S_LIST)->list;
    assert(kind);

	if (!strcmp(kind, "simple")) {
		bool flt = vx_bool_parseS(cu, snode_kv_get_expect(inner, "float"));
		size_t size;
		sscanf(snode_expect(snode_kv_get_expect(inner, "size"), S_INTEGER)->value, "%zu", &size);
		size_t align;
		sscanf(snode_expect(snode_kv_get_expect(inner, "align"), S_INTEGER)->value, "%zu", &align);

		vx_IrType* ty = fastalloc(sizeof(vx_IrType));
		ty->debugName = name;
		ty->kind = VX_IR_TYPE_KIND_BASE;
		ty->base.size = size;
		ty->base.align = align;
		ty->base.isfloat = flt;
		vx_CU_addType(cu, ty);
		return ty;
	} else {
		// TODO: add serialize support for fn types
		fprintf(stderr, "invalid type kind\n");
		exit(1);
	}
}

vx_CU* vx_CU_parseS(struct SNode* s)
{
	s = snode_expect(s, S_LIST)->list;
	char const* target = snode_expect(s, S_STRING)->value;
	s = s->next;

	vx_CU* out = malloc(sizeof(vx_CU));
	vx_CU_init(out, target);

	bool did_set_opt = false;

	for (; s; s = s->next)
	{
		struct SNode* kv = snode_expect(s, S_LIST)->list;
		char const* k = snode_expect(snode_geti_expect(kv, 0), S_SYMBOL)->value;
        assert(k);
		struct SNode* v = snode_geti_expect(kv, 1);

		if (!strcmp(k, "opt")) {
			if (did_set_opt) {
				fprintf(stderr, "opt config already set\n");
				exit(1);
			}

			vx_OptConfig cfg = vx_OptConfig_parseS(out, v);
			out->opt = cfg;

			did_set_opt = true;
		}
		else if (!strcmp(k, "type")) {
			(void) vx_IrType_parseS(out, v);
		}
		else if (!strcmp(k, "cu-block")) {
			vx_CUBlock_parseS(out, v);
		}
		else {
			fprintf(stderr, "no top level element of kind %s\n", k);
			exit(1);
		}
	}

	return out;
}

//

struct SNode* vx_IrVar_emitS(vx_CU* cu, vx_IrVar v)
{
	char buf[64];
	sprintf(buf, "%zu", v);
	return snode_mk(S_INTEGER, buf);
}

struct SNode* vx_IrTypedVar_emitS(vx_CU* cu, vx_IrTypedVar tv)
{
	struct SNode* name = snode_mk(S_STRING, tv.type->debugName);
	name->next = vx_IrVar_emitS(cu, tv.var);
	return snode_mk_list(name);
}

struct SNode* vx_IrBlock_emitS(vx_CU* cu, vx_IrBlock* block);

struct SNode* vx_IrValue_emitS(vx_CU* cu, vx_IrValue val)
{
	char const * names[] = {
		[VX_IR_VAL_IMM_INT] = "int",
		[VX_IR_VAL_IMM_FLT] = "float",
		[VX_IR_VAL_VAR] = "var",
		[VX_IR_VAL_UNINIT] = "uninit",
		[VX_IR_VAL_ID] = "label",
		[VX_IR_VAL_X86_CC] = "x86-cc",
		[VX_IR_VAL_TYPE] = "type",
		[VX_IR_VAL_BLOCKREF] = "blockref",
		[VX_IR_VAL_SYMREF] = "symref",
		[VX_IR_VAL_BLOCK]= "block",
	};

	struct SNode* nd = snode_mk(S_SYMBOL, names[val.type]);

	switch (val.type) {
		case VX_IR_VAL_IMM_INT: {
			char buf[64];
			sprintf(buf, "%lli", val.imm_int);
			nd->next = snode_mk(S_INTEGER, buf);
			break;
		}

		case VX_IR_VAL_IMM_FLT:{
			char buf[64];
			sprintf(buf, "%lf", val.imm_flt);
			nd->next = snode_mk(S_FLOAT, buf);
			break;
		}

		case VX_IR_VAL_VAR: {
			nd->next = vx_IrVar_emitS(cu, val.var);
			break;
		}

		case VX_IR_VAL_UNINIT:
			break;

		case VX_IR_VAL_ID: {
			char buf[64];
			sprintf(buf, "%zu", val.id);
			nd->next = snode_mk(S_INTEGER, buf);
			break;
		}

		case VX_IR_VAL_X86_CC:
			nd->next = snode_mk(S_STRING, val.x86_cc);
			break;

		case VX_IR_VAL_TYPE:
			nd->next = snode_mk(S_STRING, val.ty->debugName);
			break;

		case VX_IR_VAL_BLOCKREF:
			nd->next = snode_mk(S_STRING, val.block->name);
			break;

		case VX_IR_VAL_SYMREF:
			nd->next = snode_mk(S_STRING, val.symref);
			break;

		case VX_IR_VAL_BLOCK:
			nd->next = vx_IrBlock_emitS(cu, val.block);
			break;
	}

	return snode_mk_list(nd);
}

struct SNode* vx_IrNamedValue_emitS(vx_CU* cu, vx_IrNamedValue v)
{
	struct SNode* nd = snode_mk(S_STRING, vx_IrName_str[v.name]);
	nd->next = vx_IrValue_emitS(cu, v.val);
	return snode_mk_list(nd);
}

struct SNode* vx_IrOp_emitS(vx_CU* cu, vx_IrOp* op)
{
	struct SNode* nd = NULL;
	nd = snode_cat(nd, snode_mk_kv("outs", snode_mk_listx(op->outs, op->outs_len, vx_IrTypedVar_emitS, cu)));
	nd = snode_cat(nd, snode_mk_kv("name", snode_mk(S_STRING, vx_IrOpType__entries[op->id].debug.a)));
	nd = snode_cat(nd, snode_mk_kv("params", snode_mk_listx(op->params, op->params_len, vx_IrNamedValue_emitS, cu)));
	nd = snode_cat(nd, snode_mk_kv("args", snode_mk_listx(op->args, op->args_len, vx_IrValue_emitS, cu)));
	return snode_mk_list(nd);
}

struct SNode* vx_IrBlock_emitS(vx_CU* cu, vx_IrBlock* block)
{
	struct SNode* nd = NULL;
	nd = snode_cat(nd, snode_mk_kv("args", snode_mk_listx(block->ins, block->ins_len, vx_IrTypedVar_emitS, cu)));
	nd = snode_cat(nd, snode_mk_kv("ops", snode_mk_listxli(block->first, next, vx_IrOp_emitS, cu)));
	nd = snode_cat(nd, snode_mk_kv("rets", snode_mk_listx(block->outs, block->outs_len, vx_IrVar_emitS, cu)));
	return snode_mk_list(nd);
}

struct SNode* vx_bool_emitS(vx_CU* cu, bool v)
{
	char str[2] = { v ? '1' : '0', '\0' };
	return snode_mk(S_INTEGER, str);
}

struct SNode* vx_CUBlock_emitS(vx_CU* cu, vx_CUBlock* block)
{
	// TODO: other block types
	assert(block->type == VX_CU_BLOCK_IR);

	struct SNode* attribs = NULL;
	attribs = snode_cat(attribs, snode_mk_kv("name", snode_mk(S_STRING, block->v.ir->name)));
	attribs = snode_cat(attribs, snode_mk_kv("export", vx_bool_emitS(cu, block->do_export)));

	struct SNode* nd = NULL;
	nd = snode_cat(nd, snode_mk_list(attribs));
	nd = snode_cat(nd, snode_mk_list(
				snode_cat(snode_mk(S_SYMBOL, "block"), 
						  vx_IrBlock_emitS(cu, block->v.ir))));
	return snode_mk_list(nd);
}

struct SNode* vx_OptConfig_emitS(vx_CU* cu, vx_OptConfig cfg)
{
	struct SNode* nd = NULL;

	char buf[64];

	sprintf(buf, "%zu", cfg.max_total_cmov_inline_cost);
	nd = snode_cat(nd, snode_mk_kv("max_total_cmov_inline_cost", snode_mk(S_INTEGER, buf)));

	sprintf(buf, "%zu", cfg.consteval_iterations);
	nd = snode_cat(nd, snode_mk_kv("consteval_iterations", snode_mk(S_INTEGER, buf)));

	nd = snode_cat(nd, snode_mk_kv("loop_simplify", vx_bool_emitS(cu, cfg.loop_simplify)));

	nd = snode_cat(nd, snode_mk_kv("if_eval", vx_bool_emitS(cu, cfg.if_eval)));

	return snode_mk_list(nd);
}

struct SNode* vx_IrType_emitS(vx_CU* cu, vx_IrType* type)
{
	char const* kind;
	struct SNode* inner;
	switch (type->kind)
	{
		case VX_IR_TYPE_KIND_BASE:
		{
			char buf[64];
			kind = "simple";
			inner = NULL;
			inner = snode_cat(inner, snode_mk_kv("float", vx_bool_emitS(cu, type->base.isfloat)));
			sprintf(buf, "%zu", type->base.size);
			inner = snode_cat(inner, snode_mk_kv("size", snode_mk(S_INTEGER, buf)));
			sprintf(buf, "%zu", type->base.align);
			inner = snode_cat(inner, snode_mk_kv("align", snode_mk(S_INTEGER, buf)));
		}
		break;

		case VX_IR_TYPE_FUNC:
		{
			// TODO: add serialize support for fn types
			kind = "func";
			fprintf(stderr, "func types not yet support serialize\n");
			exit(1);
		}
		break;
	}

	struct SNode* out = NULL;
	out = snode_cat(out, snode_mk(S_STRING, type->debugName));
	out = snode_cat(out, snode_mk(S_SYMBOL, kind));
	out = snode_cat(out, snode_mk_list(inner));
	return snode_mk_list(out);
}

struct SNode* vx_CU_emitS(vx_CU* cu)
{
	struct SNode* list = NULL;
	list = snode_cat(list, snode_mk(S_STRING, cu->target.heap_whole));
	list = snode_cat(list, snode_mk_kv("opt", vx_OptConfig_emitS(cu, cu->opt)));

	for (size_t i = 0; i < cu->types_len; i ++) {
		list = snode_cat(list, snode_mk_kv("type", vx_IrType_emitS(cu, cu->types[i])));
	}

	for (size_t i = 0; i < cu->blocks_len; i ++) {
		list = snode_cat(list, snode_mk_kv("cu-block", vx_CUBlock_emitS(cu, &cu->blocks[i])));
	}

	return snode_mk_list(list);
}
