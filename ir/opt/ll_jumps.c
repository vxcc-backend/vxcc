#include "../passes.h"
#include <assert.h>

// we go trough all ops between the jump and the label
// if all of them have no effect (labels, nops), we can remove the jump instruction
static void part1(vx_IrBlock *block)
{
    vx_IrBlock *root = vx_IrBlock_root(block);

    for (vx_IrOp *op = block->first; op; op = op->next) {
        if (!(op->id == VX_IR_OP_GOTO || op->id == VX_IR_OP_COND))
            continue;

        size_t label_id = vx_IrOp_param(op, VX_IR_NAME_ID)->id;

        vx_IrOp *decl = root->as_root.labels[label_id].decl;
        assert (decl);
        assert (decl->parent == op->parent);

        if (vx_IrOp_after(decl, op)) // can't optimize if label decl before this decl
            continue;
        
        bool can_opt = true;
        for (vx_IrOp *other = op->next; other && other != decl; other = other->next) {
            if (vx_IrOpType_hasEffect(other->id)) {
                can_opt = false;
                break;
            }
        }

        if (!can_opt)
            continue;

        vx_IrOp_remove(op);
    }
}

// is the jump dest just another jump? optimize that
static void part2(vx_IrBlock *block)
{
    vx_IrBlock *root = vx_IrBlock_root(block);

    for (vx_IrOp *op = block->first; op; op = op->next) {
        if (!(op->id == VX_IR_OP_GOTO || op->id == VX_IR_OP_COND))
            continue;

        size_t label_id = vx_IrOp_param(op, VX_IR_NAME_ID)->id;

        vx_IrOp *decl = root->as_root.labels[label_id].decl;
        if (decl->parent != block)
            continue;

        if (decl->next) {
            vx_IrOp *following = decl->next;

            if (following->id == VX_IR_OP_GOTO) {
                label_id = vx_IrOp_param(following, VX_IR_NAME_ID)->id;
                vx_IrOp_param(op, VX_IR_NAME_ID)->id = label_id;
            }
        }
    }
}

void vx_opt_ll_jumps(vx_CU* cu, vx_IrBlock *block)
{
    part1(block);
    part2(block);
}

