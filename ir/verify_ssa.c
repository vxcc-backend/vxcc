#include <assert.h>
#include <stdlib.h>

#include "ir.h"
#include "verify.h"

struct verify_vardecls_deeptraverse__data {
    size_t declcount;
    vx_IrVar var;
};

static bool verify_vardecls_deeptraverse(vx_IrOp *op, void *dataIn) {
    struct verify_vardecls_deeptraverse__data *va = dataIn;

    for (size_t i = 0; i < op->outs_len; i ++)
        if (op->outs[i].var == va->var)
            va->declcount ++;

    for (size_t j = 0; j < op->params_len; j ++) {
        const vx_IrValue param = op->params[j].val;

        if (param.type == VX_IR_VAL_BLOCK) {
            for (size_t k = 0; k < param.block->ins_len; k ++)
                if (param.block->ins[k].var == va->var)
                    va->declcount ++;
        }
    }

    return false;
}

vx_Errors vx_IrBlock_verify(vx_CU* cu, vx_IrBlock *block) {
    vx_Errors errors;
    errors.len = 0;
    errors.items = NULL;

    vx_IrBlock_verify_ssa_based(cu, &errors, block);

    if (block->is_root) {
        for (size_t i = 0; i < block->as_root.vars_len; i ++) {
            if (block->as_root.vars[i].decl == NULL)
                continue;

            struct verify_vardecls_deeptraverse__data dat;
            dat.var = i;
            dat.declcount = 0;
            vx_IrBlock_deepTraverse(block, verify_vardecls_deeptraverse, &dat);
            // TODO: NEED TO SEARCH FROM ROOT

            // TODO: ?
            // assert(dat.declcount > 0); // WE REMOVED VAR DECL WITHOUT REMOVING IT FROM INDEX

            if (dat.declcount > 1) {
                static char buf[256];
                sprintf(buf, "Variable %%%zu is assigned more than once!", i);
                vx_Error error = {
                    .error = "Variable assigned more than once",
                    .additional = buf
                };
                vx_Errors_add(&errors, &error);
            }
        }
    }

    return errors;
}
