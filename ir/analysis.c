#include <assert.h>

#include "ir.h"

static bool anyPlacedIter(vx_IrBlock* block) {
    for (vx_IrOp* op = block->first; op; op = op->next) {
        if (op->id == VX_IR_OP_PLACE)
            return true;

        for (size_t i = 0; i < op->params_len; i ++)
            if (op->params[i].val.type == VX_IR_VAL_BLOCK)
                if (anyPlacedIter(op->params[i].val.block))
                    return true;
    }

    return false;
}

bool vx_IrBlock_anyPlaced(vx_IrBlock* block) {
    assert(block->is_root);

    for (vx_IrVar v = 0; v < block->as_root.vars_len; v ++)
        if (block->as_root.vars[v].ever_placed)
            return true;

    return anyPlacedIter(block);
}

bool vx_IrOp_after(vx_IrOp *op, vx_IrOp *after) {
    for (; op; op = op->next)
        if (op == after)
            return true;
    return false;
}

bool vx_IrBlock_vardecl_is_in_ins(vx_IrBlock *block, vx_IrVar var) {
    vx_IrBlock *root = vx_IrBlock_root(block);
    for (size_t k = 0; k < root->ins_len; k ++) {
        if (root->ins[k].var == var) {
            return true;
        }
    }
    return false;
}

// used for C IR transforms
//
// block is optional
//
// block:
//   __  nested blocks can also exist
//  /\
//    \ search here
//     \
//     before
//
vx_IrOp *vx_IrBlock_vardecl_out_before(vx_IrBlock *block, vx_IrVar var, vx_IrOp *before) {
    if (before == NULL) {
        before = vx_IrBlock_tail(block);
    }

    for (vx_IrOp* op = vx_IrOp_predecessor(before); op; op = vx_IrOp_predecessor(op)) {
        for (size_t i = 0; i < op->outs_len; i ++)
            if (op->outs[i].var == var)
                return op;
    }

    if (block->parent == NULL)
        return NULL;

    return vx_IrBlock_vardecl_out_before(block->parent, var, block->parent_op);
}

bool vx_IrOp_ends_flow(vx_IrOp *op) {
    switch (op->id) {
    case VX_IR_OP_BREAK:
    case VX_IR_OP_CONTINUE:
    case VX_IR_OP_TAILCALL:
    case VX_IR_OP_CONDTAILCALL:
        return true;

    default:
        return false;
    }
}

bool vx_IrOp_var_used(const vx_IrOp *op, vx_IrVar var) {
    for (size_t j = 0; j < op->params_len; j++) {
        if (op->params[j].val.type == VX_IR_VAL_BLOCK) {
            if (vx_IrBlock_var_used(op->params[j].val.block, var)) {
                return true;
            }
        } else if (op->params[j].val.type == VX_IR_VAL_VAR) {
            if (op->params[j].val.var == var) {
                return true;
            }
        }
    }
    return false;
}

bool vx_IrBlock_var_used(vx_IrBlock *block, vx_IrVar var)
{
    if (block == NULL) return false;

    for (size_t i = 0; i < block->outs_len; i++)
        if (block->outs[i] == var)
            return true;

    for (vx_IrOp *op = block->first; op; op = op->next) {
        if (vx_IrOp_var_used(op, var))
            return true;
    }

    return false;
}

bool vx_IrOp_is_volatile(vx_IrOp *op)
{
    switch (op->id) {
        case VX_IR_OP_IMM:
        case VX_IR_OP_REINTERPRET:
        case VX_IR_OP_ZEROEXT:
        case VX_IR_OP_SIGNEXT:
        case VX_IR_OP_TOFLT:
        case VX_IR_OP_FROMFLT:
        case VX_IR_OP_BITCAST:
        case VX_IR_OP_ADD:
        case VX_IR_OP_SUB:
        case VX_IR_OP_MUL:
        case VX_IR_OP_UDIV:
        case VX_IR_OP_SDIV:
        case VX_IR_OP_MOD:
        case VX_IR_OP_UGT:
        case VX_IR_OP_UGTE:
        case VX_IR_OP_ULT:
        case VX_IR_OP_ULTE:
        case VX_IR_OP_SGT:
        case VX_IR_OP_SGTE:
        case VX_IR_OP_SLT:
        case VX_IR_OP_SLTE:
        case VX_IR_OP_EQ:
        case VX_IR_OP_NEQ:
        case VX_IR_OP_NOT:
        case VX_IR_OP_AND:
        case VX_IR_OP_OR:
        case VX_IR_OP_BITWISE_NOT:
        case VX_IR_OP_BITWISE_AND:
        case VX_IR_OP_BITIWSE_OR:
        case VX_IR_OP_SHL:
        case VX_IR_OP_SHR:
        case VX_IR_OP_FOR:
        case VX_IR_OP_LOAD:
        case VX_IR_OP_CMOV:
        case VX_IR_OP_BITMASK:
        case VX_IR_OP_BITEXTRACT:
        case VX_IR_OP_BITPOPCNT: 
        case VX_IR_OP_BITTZCNT:
        case VX_IR_OP_BITLZCNT:
        case VX_IR_OP_EA:
        case VX_IR_OP_NEG:
            return false;

        case VX_IR_OP_BREAK:
        case VX_IR_OP_CONTINUE:
        case VX_IR_OP_CALL:
        case VX_IR_OP_TAILCALL:
        case VX_IR_OP_CONDTAILCALL:
            return true;

        case VX_IR_OP_STORE:
        case VX_IR_OP_PLACE: 
            return true;

        case VX_IR_OP_LOAD_VOLATILE:
        case VX_IR_OP_STORE_VOLATILE:
            return true;
        
        case VX_IR_OP_REPEAT:
        case VX_CIR_OP_CFOR:
        case VX_IR_OP_IF:
        case VX_IR_OP_FLATTEN_PLEASE:
        case VX_IR_OP_INFINITE:
        case VX_IR_OP_WHILE:
        case VX_IR_OP_FOREACH:
        case VX_IR_OP_FOREACH_UNTIL:
        case VX_IR_OP_VSCALE:
            for (size_t i = 0; i < op->params_len; i ++)
                if (op->params[i].val.type == VX_IR_VAL_BLOCK)
                    if (vx_IrBlock_is_volatile(op->params[i].val.block))
                        return true;
            return false;

        case VX_LIR_COND:
        case VX_LIR_GOTO:
        case VX_LIR_OP_LABEL:
            return true;

        case VX_IR_OP____END:
	default:
	    assert(false);
            return false;
    }
}

bool vx_IrBlock_is_volatile(vx_IrBlock *block)
{
    for (vx_IrOp *op = block->first; op; op = op->next)
        if (vx_IrOp_is_volatile(op))
            return true;
    return false;
}

static vx_IrType* typeofvar(vx_IrBlock* block, vx_IrVar var) {
    if (!block) {
        return NULL;
    }

    for (size_t i = 0; i < block->ins_len; i ++) {
        if (block->ins[i].var == var) {
            return block->ins[i].type;
        }
    }

    for (vx_IrOp* op = block->first; op; op = op->next) {
        for (size_t i = 0; i < op->outs_len; i ++)
            if (op->outs[i].var == var)
                return op->outs[i].type;
    
        for (size_t i = 0; i < op->params_len; i ++) {
            if (op->params[i].val.type == VX_IR_VAL_BLOCK) {
                vx_IrType* ty = typeofvar(op->params[i].val.block, var);
                if (ty) return ty;
            }
        }
    }

    return NULL;
}

vx_IrType *vx_IrBlock_typeof_var(vx_IrBlock *block, vx_IrVar var) {
    vx_IrBlock* root = vx_IrBlock_root(block);
    if (root && root->is_root && root->as_root.vars[var].ll_type) {
        return block->as_root.vars[var].ll_type;
    }

    return typeofvar(root, var);
}

static size_t cost_lut[VX_IR_OP____END] = {
    [VX_IR_OP_IMM] = 1,
    [VX_IR_OP_FLATTEN_PLEASE] = 0,
    [VX_IR_OP_REINTERPRET] = 0,
    [VX_IR_OP_ZEROEXT] = 1,
    [VX_IR_OP_SIGNEXT] = 1,
    [VX_IR_OP_TOFLT] = 1,
    [VX_IR_OP_FROMFLT] = 1,
    [VX_IR_OP_BITCAST] = 0,
    [VX_IR_OP_LOAD] = 1,
    [VX_IR_OP_LOAD_VOLATILE] = 1,
    [VX_IR_OP_STORE] = 1,
    [VX_IR_OP_STORE_VOLATILE] = 1,
    [VX_IR_OP_PLACE] = 0,
    [VX_IR_OP_ADD] = 1,
    [VX_IR_OP_SUB] = 1,
    [VX_IR_OP_MUL] = 1,
    [VX_IR_OP_UDIV] = 1,
    [VX_IR_OP_SDIV] = 1,
    [VX_IR_OP_MOD] = 1,
    [VX_IR_OP_UGT] = 1,
    [VX_IR_OP_UGTE] = 1,
    [VX_IR_OP_ULT] = 1,
    [VX_IR_OP_ULTE] = 1,
    [VX_IR_OP_SGT] = 1,
    [VX_IR_OP_SGTE] = 1,
    [VX_IR_OP_SLT] = 1,
    [VX_IR_OP_SLTE] = 1,
    [VX_IR_OP_EQ] = 1,
    [VX_IR_OP_NEQ] = 1,
    [VX_IR_OP_NOT] = 1,
    [VX_IR_OP_AND] = 1,
    [VX_IR_OP_OR] = 1,
    [VX_IR_OP_BITWISE_NOT] = 1,
    [VX_IR_OP_BITWISE_AND] = 1,
    [VX_IR_OP_BITIWSE_OR] = 1,
    [VX_IR_OP_SHL] = 1,
    [VX_IR_OP_FOR] = 1,
    [VX_IR_OP_INFINITE] = 1,
    [VX_IR_OP_WHILE] = 1,
    [VX_IR_OP_CONTINUE] = 1,
    [VX_IR_OP_BREAK] = 1,
    [VX_IR_OP_FOREACH] = 2,
    [VX_IR_OP_FOREACH_UNTIL] = 2,
    [VX_IR_OP_REPEAT] = 1,
    [VX_CIR_OP_CFOR] = 2,
    [VX_IR_OP_IF] = 1,
    [VX_IR_OP_CMOV] = 1,
    [VX_LIR_OP_LABEL] = 1,
    [VX_LIR_GOTO] = 1,
    [VX_LIR_COND] = 1,
    [VX_IR_OP_CALL] = 1,
    [VX_IR_OP_TAILCALL] = 1,
    [VX_IR_OP_CONDTAILCALL] = 1,
    [VX_IR_OP_VSCALE] = 0,
    [VX_IR_OP_BITMASK] = 1,
    [VX_IR_OP_BITEXTRACT] = 1,
    [VX_IR_OP_BITPOPCNT] = 1, 
    [VX_IR_OP_BITTZCNT] = 1,
    [VX_IR_OP_BITLZCNT] = 1,
    [VX_IR_OP_EA] = 1,
    [VX_IR_OP_NEG] = 1,
};

size_t vx_IrOp_inline_cost(vx_IrOp *op) {
    size_t total = 0;

    for (size_t i = 0; i < op->params_len; i ++)
        if (op->params[i].val.type == VX_IR_VAL_BLOCK)
            total += vx_IrBlock_inline_cost(op->params[i].val.block);

    total += cost_lut[op->id];

    return total;
}

size_t vx_IrBlock_inline_cost(vx_IrBlock *block) {
    size_t total = 0;
    for (vx_IrOp *op = block->first; op; op = op->next) {
        total += vx_IrOp_inline_cost(op);
    }
    return total;
}

vx_IrOp* vx_IrOp_nextWithEffect(vx_IrOp* op) {
    for (op = op->next; op; op = op->next)
        if (vx_IrOpType_has_effect(op->id))
            return op;
    return NULL;
}

bool vx_IrOp_followingNoEffect(vx_IrOp* op) {
    return vx_IrOp_nextWithEffect(op) == NULL;
}

static bool is_tail__rec(vx_IrBlock *block, vx_IrOp *op) {
    if (!op)
        return true;
    if (vx_IrOp_followingNoEffect(op)) {
        if (block->parent)
            return is_tail__rec(block->parent, block->parent_op);
        return true;
    }
    return false;
}

bool vx_IrOp_is_tail(vx_IrOp *op) {
    if (is_tail__rec(op->parent, op)) {
        return true;
    }

    vx_IrOp* effect = vx_IrOp_nextWithEffect(op);
    if (effect == NULL)
        return false;

    vx_IrBlock* root = vx_IrBlock_root(op->parent);
    assert(root);

    if (effect->id == VX_LIR_GOTO) {
        size_t label = vx_IrOp_param(effect, VX_IR_NAME_ID)->id;
        vx_IrOp* dest = root->as_root.labels[label].decl;
        return vx_IrOp_is_tail(dest);
    }

    return false;
}

bool vx_IrBlock_ll_isleaf(vx_IrBlock* block) {
    for (vx_IrOp* op = block->first; op; op = op->next) {
        switch (op->id) {
            case VX_IR_OP_CALL:
            case VX_IR_OP_CONDTAILCALL:
            case VX_IR_OP_TAILCALL:
                return false;

            default:
                break;
        }
    }

    return true;
}

