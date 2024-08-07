#include "ir.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

void vx_IrBlock_rename_var(vx_IrBlock *block, vx_IrVar old, vx_IrVar new) {
    for (size_t j = 0; j < block->ins_len; j ++)
        if (block->ins[j].var == old)
            block->ins[j].var = new;

    for (size_t j = 0; j < block->outs_len; j ++)
        if (block->outs[j] == old)
            block->outs[j] = new;

    vx_IrBlock *root = vx_IrBlock_root(block);
    assert(root->is_root);

    root->as_root.vars[old].decl = NULL;

    for (vx_IrOp *op = block->first; op; op = op->next) {
        for (size_t j = 0; j < op->outs_len; j ++)
            if (op->outs[j].var == old)
                op->outs[j].var = new;

        for (size_t j = 0; j < op->params_len; j ++) {
            if (op->params[j].val.type == VX_IR_VAL_VAR && op->params[j].val.var == old) {
                op->params[j].val.var = new;
            }
            else if (op->params[j].val.type == VX_IR_VAL_BLOCK) {
                vx_IrBlock *child = op->params[j].val.block;
                vx_IrBlock_rename_var(child, old, new);
            }
        }
    }
}

