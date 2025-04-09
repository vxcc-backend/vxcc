#ifndef IR_H
#define IR_H

#if defined(__LM__)
# define bool int
# define true 1
# define false 0
#else
# include <stdbool.h>
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include "../targets/targets.h"
#include "../build/ir/ops.cdef.h"
#include "../s-expr/sexpr.h"

typedef size_t vx_IrVar;

/** alloc things that are not meant to be freed or reallocated before end of compilation */
void * fastalloc(size_t bytes);
void * fastrealloc(void * old, size_t oldBytes, size_t newBytes);
void   fastfreeall(void);
char * faststrdup(const char * str);

typedef struct {
    const char *error;
    const char *additional;
} vx_Error;

typedef struct {
    vx_Error *items;
    size_t       len;
} vx_Errors;

void vx_Errors_add(vx_Errors *errors, const vx_Error *error);
void vx_Errors_add_all_and_free(vx_Errors *dest, vx_Errors *src);
void vx_Errors_free(vx_Errors errors);
void vx_Errors_print(vx_Errors errors, FILE *dest);

struct vx_IrOp_s;
typedef struct vx_IrOp_s vx_IrOp;

typedef struct {
    vx_IrVar var;
    bool present;
} vx_OptIrVar;

#define VX_IRVAR_OPT_NONE (vx_OptIrVar) {.present = false,.var = 0}
#define VX_IRVAR_OPT_SOME(v) (vx_OptIrVar) {.present = true,.var = v}

const char * vx_OptIrVar_debug(vx_OptIrVar var);

typedef struct vx_IrType_s vx_IrType;

typedef struct {
    size_t size;
    size_t align;
    bool   isfloat;
} vx_IrTypeBase;

typedef struct {
    vx_IrType **args;
    size_t      args_len;

    vx_IrType **rets;
	size_t      rets_len;
} vx_IrTypeFunc;

typedef enum {
    // present in: cir, ssa
    VX_IR_TYPE_KIND_BASE,
    VX_IR_TYPE_FUNC, // TODO: rename to VX_IR_TYPE_KIND_FUNC
} vx_IrTypeKind;

struct vx_IrType_s {
	// TODO: rename to `name` and ensure that is unique
    const char *debugName;

    vx_IrTypeKind kind;

    union {
        vx_IrTypeBase       base;
        vx_IrTypeFunc       func;
    };
};

vx_IrType* vx_IrType_heap(void);

typedef struct {
    vx_IrVar var;
    vx_IrType *type;
} vx_IrTypedVar;

// TODO: re-name to alive_in_op
typedef struct {
    bool * used_in_op;
} lifetime;

struct vx_IrBlock_s;
typedef struct vx_IrBlock_s vx_IrBlock;

struct vx_IrBlock_s {
    vx_IrBlock *parent;
    vx_IrOp    *parent_op;

    bool is_root;
    struct {
        struct {
            vx_IrOp *decl;

            lifetime  ll_lifetime;
            vx_IrType *ll_type;
            bool ever_placed;

			size_t heat;
        } *vars;
        size_t vars_len;

        struct {
            vx_IrOp *decl;
        } *labels;
        size_t labels_len;
    } as_root;

    vx_IrTypedVar *ins;
    size_t    ins_len;

    vx_IrOp *first;

    vx_IrVar *outs;
    size_t    outs_len;

    vx_IrType ** ll_out_types;
    size_t       ll_out_types_len;

    bool should_free;

    const char *name;

	void* backend;
};

typedef enum {
    VX_CU_BLOCK_IR,
    VX_CU_BLOCK_BLK_REF,
    VX_CU_BLOCK_ASM,
    VX_CU_BLOCK_DATA,
	// TODO: DATA_REF
} vx_CUBlockType;

typedef struct {
	// TODO: in the future: use vector types for data 

    char const* name;

    size_t      numel;
    vx_IrType * elty;
    size_t      comptime_elt_size;
    /** if null, then extern data */
    void      * data;
	bool        writable;
} vx_Data;

typedef struct {
    const char * name;

    size_t      ins_len;
    vx_IrType * ins;

    size_t outs_len;
    vx_IrType * outs;
} vx_RefBlock;

typedef struct {
    vx_CUBlockType type;
    union {
        vx_IrBlock  * ir;
        vx_RefBlock * blk_ref;
        char *        asmb; // ptr to malloc()-ed asm source code
        vx_Data     * data;
    } v;
    bool       do_export;
} vx_CUBlock;

typedef struct {
    size_t max_total_cmov_inline_cost;
    size_t consteval_iterations;
    bool   loop_simplify;
    bool   if_eval;
} vx_OptConfig;

typedef struct vx_CU vx_CU;
typedef struct vx_IrValue vx_IrValue;

typedef struct {
	void* tg;

    bool cmov_opt;
    bool tailcall_opt;
    bool ea_opt;
    bool (*need_move_ret_to_arg)(vx_CU* cu, vx_IrBlock* block, size_t ret_id);

	/** gets a pointer type valid for the given root block; result should be cached! */
	vx_IrType* (*get_ptr_ty)(vx_CU*, vx_IrBlock* root);

	/** appends instructions to [dest] to generate a vx_IrValue that is of type [get_ptr_ty] and represents a invalid pointer */
	vx_IrValue (*get_null_ptr)(vx_CU* cu, vx_IrBlock* dest);

	/** expand pointer addition or subtraction at this op; called in ssair */
	void (*lower_ptr_math)(vx_CU* cu, vx_IrOp* op);

	/** append instructions to the end of the block to generate operations that convert a pointer to a integer which is only used in things like printf("%p") */
	vx_IrValue (*cast_ptr_to_human)(vx_CU* cu, vx_IrBlock* block, vx_IrVar ptr);
} vx_TargetInfo;

void vx_Target_info(vx_TargetInfo* dest, vx_Target const* target);

/** single compilation unit */
struct vx_CU {
    vx_Target     target;
    vx_TargetInfo info;

    vx_OptConfig  opt;

    // TODO: rename to symbols 
    vx_CUBlock * blocks;
    size_t       blocks_len;

	vx_IrType ** types;
	size_t types_len;
};

/** targetStr is "arch:ext1,ext2" or "arch" */
void vx_CU_init(vx_CU* dest, const char * targetStr);

vx_CUBlock* vx_CU_addBlock(vx_CU* vx_cu);

void vx_CU_addIrBlock(vx_CU* vx_cu, vx_IrBlock* block, bool doExport);

static void vx_CU_addType(vx_CU* cu, vx_IrType* type) {
	cu->types = (vx_IrType**) realloc(cu->types, sizeof(vx_IrType*) * (cu->types_len + 1));
	cu->types[cu->types_len ++] = type;
}

vx_IrType* vx_CU_typeByName(vx_CU* cu, char const* name);
vx_IrBlock* vx_CU_blockByName(vx_CU* cu, char const* name);

typedef enum { 
    VX_CU_COMPILE_MODE_FROM_LLIR = 0,
    VX_CU_COMPILE_MODE_FROM_IR,
    VX_CU_COMPILE_MODE_FROM_CIR,
} vx_CU_compile_Mode;

/** 0 if ok */
int vx_CU_compile(vx_CU * cu,
                  FILE* optionalOptimizedSsaIr,
                  FILE* optionalOptimizedLlIr,
                  FILE* optionalAsm,
                  vx_BinFormat optionalBinFormat, FILE* optionalBinOut,
                  vx_CU_compile_Mode mode);

vx_IrBlock *vx_IrBlock_root(vx_IrBlock *block);

// TODO: do differently

vx_Errors vx_IrBlock_verify(vx_CU* cu, vx_IrBlock *block);

int vx_ir_verify(vx_CU* cu, vx_IrBlock *block);

struct vx_IrValue {
    enum {
        // storable
        VX_IR_VAL_IMM_INT,
        VX_IR_VAL_IMM_FLT,
        VX_IR_VAL_VAR,
        VX_IR_VAL_UNINIT,
        VX_IR_VAL_BLOCKREF,
		VX_IR_VAL_SYMREF,

        // not storable
        VX_IR_VAL_BLOCK,
        VX_IR_VAL_TYPE,
        VX_IR_VAL_ID,
		VX_IR_VAL_X86_CC,
    } type;

    vx_IrType* no_read_rt_type;

    union {
        long long imm_int;
        double imm_flt;
        vx_IrVar var;

        vx_IrBlock *block;
        vx_IrType *ty;
        size_t id;
		const char * symref;

		char x86_cc[4];
    };
};

bool vx_IrValue_eq(vx_IrValue a, vx_IrValue b);

#define VX_IR_VALUE_IMM_INT(varin) ((vx_IrValue) { .type = VX_IR_VAL_IMM_INT, .imm_int = varin })
#define VX_IR_VALUE_IMM_FLT(varin) ((vx_IrValue) { .type = VX_IR_VAL_IMM_FLT, .imm_flt = varin })
#define VX_IR_VALUE_VAR(varin)  ((vx_IrValue) { .type = VX_IR_VAL_VAR, .var = varin })
#define VX_IR_VALUE_UNINIT()    ((vx_IrValue) { .type = VX_IR_VAL_UNINIT })
#define VX_IR_VALUE_BLKREF(blk) ((vx_IrValue) { .type = VX_IR_VAL_BLOCKREF, .block = blk })
#define VX_IR_VALUE_TYPE(idin)  ((vx_IrValue) { .type = VX_IR_VAL_TYPE, .ty = idin })
#define VX_IR_VALUE_BLK(blk)    ((vx_IrValue) { .type = VX_IR_VAL_BLOCK, .block = blk })
#define VX_IR_VALUE_ID(idin)    ((vx_IrValue) { .type = VX_IR_VAL_ID, .id = idin })
static vx_IrValue VX_IR_VALUE_X86_CC(char const * cc) {
	vx_IrValue res; res.type = VX_IR_VAL_X86_CC;
	memcpy(res.x86_cc, cc, 4);
	res.x86_cc[3] = '\0';
	return res;
}
#define VX_IR_VALUE_SYMREF(sy)  ((vx_IrValue) { .type = VX_IR_VAL_SYMREF, .symref = sy })

void vx_IrValue_dump(vx_IrValue value, FILE *out, size_t indent);

/**
 * used for C IR transforms
 *
 * block is optional
 *
 * block:
 *   __  nested blocks can also exist
 *  /\
 *    \ search here
 *     \
 *   before
*/ 
vx_IrOp *vx_IrBlock_vardeclOutBefore(vx_IrBlock *block, vx_IrVar var, vx_IrOp *before);
vx_IrOp *vx_IrBlock_tail(vx_IrBlock *block);
/** DON'T RUN INIT AFTERWARDS */
vx_IrBlock *vx_IrBlock_initHeap(vx_IrBlock *parent, vx_IrOp *parent_op);
void vx_IrBlock_init(vx_IrBlock *block, vx_IrBlock *parent, vx_IrOp *parent_op);
/** run AFTER you finished building it! */
void vx_IrBlock_makeRoot(vx_IrBlock *block, size_t total_vars);
void vx_IrBlock_addIn(vx_IrBlock *block, vx_IrVar var, vx_IrType *type);
void vx_IrBlock_addOp(vx_IrBlock *block, const vx_IrOp *op);
/** WARNING: DON'T REF VARS IN OP THAT ARE NOT ALREADY INDEXED ROOT */
vx_IrOp *vx_IrBlock_addOpBuilding(vx_IrBlock *block);
vx_IrOp *vx_IrBlock_insertOpCreateAfter(vx_IrBlock* block, vx_IrOp* afterNullable, vx_IrOpType ty);
void vx_IrBlock_addAllOp(vx_IrBlock *dest, const vx_IrBlock *src);
void vx_IrBlock_insertAllOpAfter(vx_IrBlock* dest, const vx_IrBlock* src, vx_IrOp* afterNullable);
void vx_IrBlock_addOut(vx_IrBlock *block, vx_IrVar out);
void vx_IrBlock_destroy(vx_IrBlock *block);
vx_IrType *vx_IrBlock_typeofVar(vx_IrBlock *block, vx_IrVar var);
vx_IrVar vx_IrBlock_newVar(vx_IrBlock *block, vx_IrOp *decl);
size_t   vx_IrBlock_newLabel(vx_IrBlock *block, vx_IrOp *decl);
void vx_IrBlock_swapInAt(vx_IrBlock *block, size_t a, size_t b);
void vx_IrBlock_swapOutAt(vx_IrBlock *block, size_t a, size_t b);
void vx_IrBlock_removeOutAt(vx_IrBlock *block, size_t id);
size_t vx_IrBlock_appendLabelOp(vx_IrBlock *block);
/** for when you aquired a label id via vx_IrBlock_new_label(?, NULL) and want to add a label op at the tail*/
void vx_IrBlock_appendLabelOpPredefined(vx_IrBlock *block, size_t id);
void vx_IrBlock_putVar(vx_IrBlock *root, vx_IrVar var, vx_IrOp *decl);
void vx_IrBlock_putLabel(vx_IrBlock *root, size_t label, vx_IrOp *decl);
bool vx_IrBlock_anyPlaced(vx_IrBlock* block); // only for root blocks
bool vx_IrBlock_empty(vx_IrBlock *block);
bool vx_IrBlock_varUsed(vx_IrBlock *block, vx_IrVar var);
void vx_IrBlock_dump(vx_IrBlock *block, FILE *out, size_t indent);
/** returns true if static eval ok; only touches dest if true */
bool vx_IrBlock_evalVar(vx_IrBlock *block, vx_IrVar var, vx_IrValue *dest);
bool vx_Irblock_mightbe(vx_IrBlock *block, vx_IrValue val, vx_IrValue v);
bool vx_Irblock_alwaysIs(vx_IrBlock *block, vx_IrValue val, vx_IrValue v);
void vx_Irblock_eval(vx_IrBlock *block, vx_IrValue *v);

/** can be ored together */ 
typedef enum { VX_RENAME_VAR_OUTPUTS = 0b1, VX_RENAME_VAR_INPUTS = 0b10, VX_RENAME_VAR_BOTH = 0b11 } vx_RenameVarCfg;
/** ret val: did rename any? **/
bool vx_IrBlock_renameVar(vx_IrBlock *block, vx_IrVar old, vx_IrVar newv, vx_RenameVarCfg cfg);
bool vx_IrBlock_deepTraverse(vx_IrBlock *block, bool (*callback)(vx_IrOp *op, void *data), void *data);
bool vx_IrBlock_vardeclIsInIns(vx_IrBlock *block, vx_IrVar var);
vx_IrOp* vx_IrBlock_varDeclRec(vx_IrBlock *block, vx_IrVar var);
vx_IrOp* vx_IrBlock_varDeclNoRec(vx_IrBlock *block, vx_IrVar var);
/** might not contain unique elemes */
vx_IrVar* vx_IrBlock_listDeclaredVarsRec(vx_IrBlock *block, size_t * listLenOut);

typedef enum {
    VX_IR_NAME_OPERAND_A,
    VX_IR_NAME_OPERAND_B,

    VX_IR_NAME_BLOCK,
    VX_IR_NAME_VALUE,
    VX_IR_NAME_ADDR,
    VX_IR_NAME_COND,
    VX_IR_NAME_VAR,
    VX_IR_NAME_SIZE,

    VX_IR_NAME_COND_THEN,
    VX_IR_NAME_COND_ELSE,

    VX_IR_NAME_LOOP_DO,
    VX_IR_NAME_LOOP_START,
    VX_IR_NAME_LOOP_ENDEX,
    VX_IR_NAME_LOOP_STRIDE,

    VX_IR_NAME_IDX,
    VX_IR_NAME_TYPE,
    VX_IR_NAME_ELSIZE,
    VX_IR_NAME_OFF,

    VX_IR_NAME_ID,

    VX_IR_NAME__LAST,
} vx_IrName;

extern const char *vx_IrName_str[];

vx_IrName vx_IrName_parse(const char * src, uint32_t srcLen);
vx_IrName vx_IrName_parsec(const char * src);

bool vx_IrOpType_parse(vx_IrOpType* dest, const char * name, size_t name_len);
bool vx_IrOpType_parsec(vx_IrOpType* dest, const char * name);

typedef struct {
    vx_IrName   name;
    vx_IrValue  val;
} vx_IrNamedValue;

vx_IrNamedValue vx_IrNamedValue_create(vx_IrName name, vx_IrValue v);
void vx_IrNamedValue_destroy(vx_IrNamedValue v);

struct vx_IrOp_s {
    vx_IrOp *next;

    vx_IrTypedVar *outs;
    size_t         outs_len;

    vx_IrNamedValue *params;
    size_t           params_len;

    vx_IrValue  * args;
    size_t        args_len;

    vx_IrBlock  * parent;
    vx_IrOpType   id;

	void* backend;
};

vx_IrOp* vx_IrBlock_lastOfType(vx_IrBlock* block, vx_IrOpType type);

#define MKARR(...) { __VA_ARGS__ }

#define FOR_PARAMS(op,want,paramn,fn) { \
    vx_IrName __names[] = want; \
    for (size_t __it = 0; __it < sizeof(__names) / sizeof(vx_IrName); __it ++) { \
        vx_IrValue *__param = vx_IrOp_param(op, __names[__it]); \
        if (__param) {\
            vx_IrValue paramn = *__param; \
            fn; \
        }\
    } \
}

#define FOR_INPUTS_REF(op,inp,fn) { \
    for (size_t __it = 0; __it < op->args_len; __it ++) {\
        vx_IrValue* inp = &op->args[__it]; \
        fn; \
    } \
    for (size_t __it = 0; __it < op->params_len; __it ++) { \
        vx_IrValue* inp = &op->params[__it].val; \
        fn; \
    } \
}

#define FOR_INPUTS(op,inp,fn) FOR_INPUTS_REF(op, __ref, { vx_IrValue inp = *__ref; fn; });

size_t vx_IrOp_countSuccessors(vx_IrOp *op);
size_t vx_IrBlock_countOps(vx_IrBlock *block);

vx_IrOp *vx_IrOp_predecessor(vx_IrOp *op);
void vx_IrOp_removeSuccessor(vx_IrOp *op);
/** should use remove_successor whenever possible! */
void vx_IrOp_remove(vx_IrOp *op);

/** false for nop and label   true for everything else */
bool vx_IrOpType_hasEffect(vx_IrOpType type);
bool vx_IrOp_endsFlow(vx_IrOp *op);
bool vx_IrOp_isVolatile(vx_IrOp *op);
bool vx_IrOp_hasSideEffect(vx_IrOp *op);
size_t vx_IrOp_inlineCost(vx_IrOp *op);
size_t vx_IrOp_execCost(vx_IrOp *op);

bool vx_IrBlock_endsFlow(vx_IrBlock *block);
bool vx_IrBlock_isVolatile(vx_IrBlock *block);
bool vx_IrBlock_hasSideEffect(vx_IrBlock *block);
size_t vx_IrBlock_inlineCost(vx_IrBlock *block);
size_t vx_IrBlock_execCost(vx_IrBlock *block);

void vx_IrOp_undeclare(vx_IrOp *op);
bool vx_IrOp_varUsed(const vx_IrOp *op, vx_IrVar var);
bool vx_IrOp_varInOuts(const vx_IrOp *op, vx_IrVar var);
void vx_IrOp_warn(vx_IrOp *op, const char *optMsg0, const char *optMsg1);
void vx_IrOp_dump(const vx_IrOp *op, FILE *out, size_t indent);
vx_IrValue *vx_IrOp_param(const vx_IrOp *op, vx_IrName name);
void vx_IrOp_init(vx_IrOp *op, vx_IrOpType type, vx_IrBlock *parent);
void vx_IrOp_addOut(vx_IrOp *op, vx_IrVar v, vx_IrType *t);
void vx_IrOp_addParam_s(vx_IrOp *op, vx_IrName name, vx_IrValue val);
void vx_IrOp_addParam(vx_IrOp *op, vx_IrNamedValue p);
void vx_IrOp_addArg(vx_IrOp *op, vx_IrValue val);
void vx_IrOp_stealParam(vx_IrOp *dest, const vx_IrOp *src, vx_IrName param);
void vx_IrOp_removeParams(vx_IrOp *op);
void vx_IrOp_removeOutAt(vx_IrOp *op, size_t id);
void vx_IrOp_removeParamAt(vx_IrOp *op, size_t id);
void vx_IrOp_removeParam(vx_IrOp *op, vx_IrName name);
void vx_IrOp_stealOuts(vx_IrOp *dest, const vx_IrOp *src);
void vx_IrOp_destroy(vx_IrOp *op);
void vx_IrOp_removeArgAt(vx_IrOp *op, size_t id);
void vx_IrOp_stealArgs(vx_IrOp *dest, const vx_IrOp *src);
bool vx_IrOp_isTail(vx_IrOp *op);
bool vx_IrOp_after(vx_IrOp *op, vx_IrOp *after);
bool vx_IrOp_followingNoEffect(vx_IrOp* op);
vx_IrOp* vx_IrOp_nextWithEffect(vx_IrOp* op);
void vx_IrOp_updateParent(vx_IrOp* op, vx_IrBlock* to);

typedef bool (*vx_IrOpFilter)(vx_IrOp* op, void* userdata);

bool VX_IR_OPFILTER_ID__impl(vx_IrOp*,void*);
#define VX_IR_OPFILTER_ID(id) \
    VX_IR_OPFILTER_ID__impl, \
    (void*) (intptr_t) id

bool VX_IR_OPFILTER_COMPARISION__impl(vx_IrOp*,void*);
#define VX_IR_OPFILTER_COMPARISION \
    VX_IR_OPFILTER_COMPARISION__impl, \
    NULL 

bool VX_IR_OPFILTER_CONDITIONAL__impl(vx_IrOp*,void*);
#define VX_IR_OPFILTER_CONDITIONAL \
    VX_IR_OPFILTER_CONDITIONAL__impl, \
    NULL

bool VX_IR_OPFILTER_PURE__impl(vx_IrOp*,void*);
#define VX_IR_OPFILTER_PURE \
    VX_IR_OPFILTER_PURE__impl, \
    NULL

bool VX_IR_OPFILTER_BOTH__impl(vx_IrOp*,void*);
#define VX_IR_OPFILTER_BOTH(a, b) \
    VX_IR_OPFILTER_BOTH__impl, \
    (void*[]) { a, b }

bool vx_IrBlock_nextOpListBetween(vx_IrBlock* block,
                                  vx_IrOp** first, vx_IrOp** last,
                                  vx_IrOpFilter matchBegin, void *data0,
                                  vx_IrOpFilter matchEnd, void *data1);

vx_IrOp* vx_IrOp_nextWhile(vx_IrOp* op, vx_IrOpFilter match, void *data0);
void vx_IrOp_earlierFirst(vx_IrOp** earlier, vx_IrOp** later);

bool vx_IrBlock_allMatch(vx_IrOp* first, vx_IrOp* last,
                         vx_IrOpFilter fil, void* data);

bool vx_IrBlock_noneMatch(vx_IrOp* first, vx_IrOp* last,
                          vx_IrOpFilter fil, void* data);

bool vx_IrOp_inRange(vx_IrOp* op,
                     vx_IrOp* first, vx_IrOp* last);

bool vx_IrOp_allDepsInRangeOrArgs(vx_IrBlock* block, vx_IrOp* op,
                                  vx_IrOp* first, vx_IrOp* last);

struct IrStaticIncrement {
    bool detected;
    vx_IrVar var;
    long long by;
};
struct IrStaticIncrement vx_IrOp_detectStaticIncrement(vx_IrOp *op);

vx_IrType* vx_IrBlock_type(vx_IrBlock* block);

// null if depends on context or has no type 
vx_IrType* vx_IrValue_type(vx_CU* cu, vx_IrBlock* root, vx_IrValue value);

bool vx_IrBlock_llIsLeaf(vx_IrBlock* block);

size_t vx_IrType_size(vx_CU* cu, vx_IrBlock* inCtx, vx_IrType *ty);

void vx_llir_emit_asm(vx_CU* cu, vx_IrBlock* llirblock, FILE* out);

void vx_IrBlock_markVarOrigin(vx_IrBlock* block, vx_IrVar old, vx_IrVar newv);

void vx_IrBlock_root_varsHeat(vx_IrBlock* block);
void vx_IrBlock_llir_varsHeat(vx_IrBlock* block);


vx_IrVar vx_IrVar_parseS(vx_CU* cu, struct SNode* nd);
vx_IrTypedVar vx_IrTypedVar_parseS(vx_CU* cu, struct SNode* nd);
vx_IrBlock* vx_IrBlock_parseS(vx_CU* cu, struct SNode* nd);
vx_IrValue vx_IrValue_parseS(vx_CU* cu, struct SNode* nd);
vx_IrOp* vx_IrOp_parseS(vx_CU* cu, struct SNode* s);
void vx_CUBlock_parseS(vx_CU* cu, struct SNode* s);
vx_OptConfig vx_OptConfig_parseS(vx_CU* cu, struct SNode* s);
/** also adds type to cu */
vx_IrType* vx_IrType_parseS(vx_CU* cu, struct SNode* s);
vx_CU* vx_CU_parseS(struct SNode* s);


struct SNode* vx_IrVar_emitS(vx_CU* cu, vx_IrVar v);
struct SNode* vx_IrTypedVar_emitS(vx_CU* cu, vx_IrTypedVar tv);
struct SNode* vx_IrValue_emitS(vx_CU* cu, vx_IrValue val);
struct SNode* vx_IrNamedValue_emitS(vx_CU* cu, vx_IrNamedValue v);
struct SNode* vx_IrOp_emitS(vx_CU* cu, vx_IrOp* op);
struct SNode* vx_IrBlock_emitS(vx_CU* cu, vx_IrBlock* block);
struct SNode* vx_bool_emitS(vx_CU* cu, bool v);
struct SNode* vx_CUBlock_emitS(vx_CU* cu, vx_CUBlock* block);
struct SNode* vx_OptConfig_emitS(vx_CU* cu, vx_OptConfig cfg);
struct SNode* vx_IrType_emitS(vx_CU* cu, vx_IrType* type);
struct SNode* vx_CU_emitS(vx_CU* cu);

#endif //IR_H
