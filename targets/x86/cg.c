#include "x86.h"
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

// TODO: use lea for mul sometimes 
// TODO: mul / div by power of 2
// TODO: no tailcall opt if epilog

#define PTRSIZE (8) // TODO

struct Location;

typedef struct {
    size_t id;
    const char * name[7];

    struct Location * stored;
} RegType;

#define MkRegType(idi, name0, name1, name2, name3) \
    ((RegType) { .id = (idi), .name = { name0, name1, name2, name3 }, .stored = NULL })
#define MkRegType7(idi, name0, name1, name2, name3, name4, name5, name6) \
    ((RegType) { .id = (idi), .name = { name0, name1, name2, name3, name4, name5, name6 }, .stored = NULL })
#define MkVecRegTy(id, vid) \
    MkRegType7((id), "xmm" #vid, "xmm" #vid, "xmm" #vid, "xmm" #vid, "xmm" #vid, "ymm" #vid, "zmm" #vid)

static RegType REG_RAX  = MkRegType(0,  "al",   "ax",   "eax",  "rax");
static RegType REG_RBX  = MkRegType(1,  "bl",   "bx",   "ebx",  "rbx");
static RegType REG_RCX  = MkRegType(2,  "cl",   "cx",   "ecx",  "rcx");
static RegType REG_RSP  = MkRegType(3,  "spl",  "sp",   "esp",  "rsp");
static RegType REG_RBP  = MkRegType(4,  "bpl",  "bp",   "ebp",  "rbp");
static RegType REG_RDX  = MkRegType(5,  "dl",   "dx",   "edx",  "rdx");
static RegType REG_RSI  = MkRegType(6,  "sil",  "si",   "esi",  "rsi");
static RegType REG_RDI  = MkRegType(7,  "dil",  "di",   "edi",  "rdi");
static RegType REG_R8   = MkRegType(8,  "r8b",  "r8w",  "r8d",  "r8" );
static RegType REG_R9   = MkRegType(9,  "r9b",  "r9w",  "r9d",  "r9" );
static RegType REG_R10  = MkRegType(10, "r10b", "r10w", "r10d", "r10");
static RegType REG_R11  = MkRegType(11, "r11b", "r11w", "r11d", "r11");
static RegType REG_R12  = MkRegType(12, "r12b", "r12w", "r12d", "r12");
static RegType REG_R13  = MkRegType(13, "r13b", "r13w", "r13d", "r13");
static RegType REG_R14  = MkRegType(14, "r14b", "r14w", "r14d", "r14");
static RegType REG_R15  = MkRegType(15, "r15b", "r15w", "r15d", "r15");
#define IntRegCount (16)

static RegType REG_XMM0 = MkVecRegTy(16, 0);
static RegType REG_XMM1 = MkVecRegTy(17, 1);
static RegType REG_XMM2 = MkVecRegTy(18, 2);
static RegType REG_XMM3 = MkVecRegTy(19, 3);
static RegType REG_XMM4 = MkVecRegTy(20, 4);
static RegType REG_XMM5 = MkVecRegTy(21, 5);
static RegType REG_XMM6 = MkVecRegTy(22, 6);
static RegType REG_XMM7 = MkVecRegTy(23, 7);

static RegType REG_XMM8 = MkVecRegTy(24, 8); 
static RegType REG_XMM9 = MkVecRegTy(25, 9);

#define RegCount (26)

#define IsIntReg(rid) (rid < IntRegCount)
#define VecRegId(rid) (rid - IntRegCount)

RegType* RegLut[RegCount] = {
    &REG_RAX, &REG_RBX, &REG_RCX, &REG_RSP, &REG_RBP, &REG_RDX, &REG_RSI, &REG_RDI,
    &REG_R8,  &REG_R9,  &REG_R10, &REG_R11, &REG_R12, &REG_R13, &REG_R14, &REG_R15,

    &REG_XMM0, &REG_XMM1, &REG_XMM2, &REG_XMM3,
    &REG_XMM4, &REG_XMM5, &REG_XMM6, &REG_XMM7,
};

static vx_CU* cu;
static vx_IrBlock* block;

typedef enum {
    LOC_REG = 0,
    LOC_EA,
    LOC_MEM,
    LOC_IMM,
    LOC_LABEL,
	LOC_SYM,
    LOC_INVALID,
} LocationType;

#define NEGATIVE (-1)
#define POSITIVE (1)

typedef struct Location {
    size_t bytesWidth;
    LocationType type;
    union {
        struct {
            int64_t bits;
        } imm;

        struct {
            size_t id;
        } reg;

        struct {
            struct Location* base;
            int  offsetSign;
            struct Location* offset; // nullable!
            struct Location* offsetMul; // nullable!
        } ea;

        struct {
            const char * label;
        } label;

		struct {
			const char * sym;
		} sym;

        struct {
            struct Location* address;
        } mem;
    } v;
} Location;

#define LocImm(width, value) \
    ((Location) { .bytesWidth = (width), .type = LOC_IMM, .v.imm = (value) })
#define LocReg(width, regId) \
    ((Location) { .bytesWidth = (width), .type = LOC_REG, .v.reg = (regId) })
#define LocEA(width, basee, off, sign, mul) \
    ((Location) { .bytesWidth = (width), .type = LOC_EA, .v.ea.base = (basee), .v.ea.offsetSign = (sign), .v.ea.offset = (off), .v.ea.offsetMul = (mul) })
#define LocMem(width, eaa) \
    ((Location) { .bytesWidth = (width), .type = LOC_MEM, .v.mem.address = (eaa) })
#define LocLabel(str) \
    ((Location) { .type = LOC_LABEL, .v.label.label = (str) })
#define LocSym(str) \
	((Location) { .type = LOC_SYM, .v.sym.sym = (str) })

Location* loc_opt_copy(Location* old) {
    Location* new = fastalloc(sizeof(Location));
    switch (old->type) {
        case LOC_REG: { 
                          *new = LocReg(old->bytesWidth, old->v.reg.id);
                          return new;
                      }

        case LOC_IMM: {
                          *new = LocImm(old->bytesWidth, old->v.imm.bits);
                          return new;
                      }

        case LOC_EA: {
                         if (old->v.ea.offsetMul == 0) {
                             return loc_opt_copy(old);
                         }

                         *new = LocEA(old->bytesWidth, old->v.ea.base, old->v.ea.offset, old->v.ea.offsetSign, old->v.ea.offsetMul);
                         return new;
                     }

        case LOC_MEM: {
                          *new = LocMem(old->bytesWidth, old->v.mem.address);
                          return new;
                      }

        case LOC_LABEL: {
                            *new = LocLabel(old->v.label.label);
                            return new;
                        }

		case LOC_SYM: {
			*new = LocSym(old->v.sym.sym);
			return new;
		}

        case LOC_INVALID:
        default: {
                     assert(false);
                     return NULL;
                 }
    }
}

static char widthToWidthWidth(char width) {
    static char widthToWidthWidthLut[] = {
        [0] = 0,
        [1] = 1,
        [2] = 2,
        [3] = 4,
        [4] = 4,
        [5] = 8,
        [6] = 8,
        [7] = 8,
        [8] = 8,
    };

    if (width > 8) {
        if (width > 16) {
            if (width > 32) { 
                assert(width <= 64);
                return 64;
            }
            return 32;
        }
        return 16;
    }
    return widthToWidthWidthLut[(int) width];
}

static char widthToWidthIdx(char width) {
    static char lut[] = {
        [0] = 0,
        [1] = 0,
        [2] = 1,
        [3] = 2,
        [4] = 2,
        [5] = 3,
        [6] = 3,
        [7] = 3,
        [8] = 3,
    };


    if (width > 8) {
        if (width > 16) {
            if (width > 32) { 
                assert(width <= 64);
                return 6;
            }
            return 5;
        }
        return 4;
    }

    return lut[(int) width];
}

static const char * widthWidthToASM[] = {
    [0] = "wtf",
    [1] = "byte",
    [2] = "word",
    [4] = "dword",
    [8] = "qword",
};

static bool needEpilog;

static void emit(Location*, FILE*);

static void emitEa(Location* ea, FILE* out) {
    assert(ea->v.ea.base->type != LOC_MEM);
    assert(ea->v.ea.base->type != LOC_EA);

    if (ea->v.ea.base) {
        emit(ea->v.ea.base, out);
    }
    if (ea->v.ea.offset != NULL) {
        assert(ea->v.ea.offset->type != LOC_MEM);
        assert(ea->v.ea.offset->type != LOC_EA);

        if (ea->v.ea.base) {
            if (ea->v.ea.offsetSign == NEGATIVE) {
                fputs(" - ", out);
            } else {
                fputs(" + ", out);
            }
        }
        emit(ea->v.ea.offset, out);
        if (ea->v.ea.offsetMul) {
            fputs(" * ", out);
            emit(ea->v.ea.offsetMul, out);
        }
    }
}

static void emit(Location* loc, FILE* out) {
    assert(loc);

    switch (loc->type) {
        case LOC_EA:
            emitEa(loc, out);
            break;

        case LOC_IMM:
            fprintf(out, "%zu", loc->v.imm.bits);
            break;

		case LOC_MEM: {
			char ww = widthToWidthWidth(loc->bytesWidth);
			const char * str = widthWidthToASM[(int) ww];
			fputs(str, out);
			fputs(" [", out);
			emit(loc->v.mem.address, out);
			fputs("]", out);
		} break;

        case LOC_REG:
			fputs(RegLut[loc->v.reg.id]->name[(int) widthToWidthIdx(loc->bytesWidth)], out);
			break;

        case LOC_LABEL:
			fputs(loc->v.label.label, out);
			break;

        case LOC_SYM:
			fputs(loc->v.sym.sym, out);
			break;

		case LOC_INVALID:
			assert(false);
			break;
    }
}

static Location* gen_imm_var(size_t size, int64_t bits) {
    Location* l = fastalloc(sizeof(Location));
    *l = LocImm(size, bits);
    return l;
}

static Location* gen_reg_var(size_t size, size_t regId) {
    Location* l = fastalloc(sizeof(Location));
    *l = LocReg(size, regId);
    return l;
}

static Location* gen_mem_var(size_t size, Location* addr) {
    Location* l = fastalloc(sizeof(Location));
    *l = LocMem(size, addr);
    return l;
}

static Location* gen_stack_var(size_t varSize, size_t stackOffset) {
    Location* reg = gen_reg_var(PTRSIZE, REG_RBP.id);
    Location* off = gen_imm_var(PTRSIZE, varSize + stackOffset);

    Location* ea = fastalloc(sizeof(Location));
    *ea = LocEA(varSize, reg, off, NEGATIVE, NULL);

    return gen_mem_var(varSize, ea);
}

static Location* start_scratch_reg(size_t size, FILE* out);
static void end_scratch_reg(Location* scratch, FILE* out);

static Location* start_as_dest_reg(Location* other, FILE* out);
static void end_as_dest_reg(Location* as_reg, Location* old, FILE* out);

static Location* start_as_primitive(Location* other, FILE* out);
static void end_as_primitive(Location* as_prim, Location* old, FILE* out);

static Location* start_as_size(size_t size, Location* loc, FILE* out);
static void end_as_size(Location* as_size, Location* old, FILE* out);

static void emiti_move(Location* src, Location* dest, bool sign_ext, FILE* out);

static void emiti_lea(Location* src, Location* dest, FILE* out) {
    Location* dest_as_reg = start_as_dest_reg(dest, out);

    assert(src->type == LOC_EA);

    fputs("lea ", out);
    emit(dest_as_reg, out);
    fputs(", [", out);
    emitEa(src, out);
    fputs("]\n", out);

    end_as_dest_reg(dest_as_reg, dest, out);
} 

static void emiti_enter(FILE* out) {
    fputs("push rbp\n", out);
    fputs("mov rbp, rsp\n", out);
} 

static void emiti_leave(FILE* out) {
    fputs("pop rbp\n", out);
}

static void emiti_jump_cond(Location* target, const char *cc, FILE* out) {
    Location* as_prim = start_as_primitive(target, out);

    fprintf(out, "j%s ", cc);
    emit(as_prim, out);
    fputc('\n', out);

    end_as_primitive(as_prim, target, out);
}

static void emiti_jump(Location* target, FILE* out) {
    emiti_jump_cond(target, "mp" /* jmp */, out);
}

static void emiti_call(Location* target, FILE* out) {
    Location* as_prim = start_as_primitive(target, out);

    fputs("call ", out);
    emit(as_prim, out);
    fputc('\n', out);

    end_as_primitive(as_prim, target, out);
}

static void emiti_storecond(Location* dest, const char *cc, FILE* out) {
    if (dest->bytesWidth == 1) {
        fprintf(out, "set%s ", cc);
        emit(dest, out);
        fputc('\n', out);
    }
    else {
        Location* scratch = start_scratch_reg(1, out);
        emiti_storecond(scratch, cc, out);
        emiti_move(scratch, dest, /* sign extend */ false, out);
        end_scratch_reg(scratch, out);
    }
}

static void emiti_zero(Location* dest, FILE* out) {
    if (dest->type == LOC_REG) {
        if (IsIntReg(dest->v.reg.id)) {
            if (dest->bytesWidth == 8) {
                dest = loc_opt_copy(dest);
                dest->bytesWidth = 4;
            }

            fputs("xor ", out);
            emit(dest, out);
            fputs(", ", out);
            emit(dest, out);
            fputc('\n', out);
        } else {
            fputs("pxor ", out);
            emit(dest, out);
            fputs(", ", out);
            emit(dest, out);
            fputc('\n', out);
        }
        return;
    }

    fputs("mov ", out);
    emit(dest, out);
    fputs(", 0\n", out);
}

static bool equal(Location* a, Location* b) {
    if (a == NULL || b == NULL)
        return false;

    if (a == b)
        return true;

    if (a->type != b->type)
        return false;

    if (a->type == LOC_REG && a->v.reg.id == b->v.reg.id)
        return true;

    if (a->type == LOC_MEM && equal(a->v.mem.address, b->v.mem.address))
        return true;

    if (a->type == LOC_IMM && a->v.imm.bits == b->v.imm.bits)
        return true;

    if (a->type == LOC_EA &&
            equal(a->v.ea.base, b->v.ea.base) &&
            equal(a->v.ea.offset, b->v.ea.offset) && 
            a->v.ea.offsetMul == b->v.ea.offsetMul &&
            a->v.ea.offsetSign == b->v.ea.offsetSign)
        return true;

    return false;
}

static void emiti_move(Location* src, Location *dest, bool sign_ext, FILE* out) {
    assert(src);
    assert(dest);

    if (src->type == LOC_INVALID) { // undefined means that we don't have to set it at all
        return;
    }

    if (src->type == LOC_IMM && src->v.imm.bits == 0) {
        emiti_zero(dest, out);
        return;
    }

    if (equal(src, dest)) return;

    if (src->type == LOC_EA) {
        emiti_lea(src, dest, out);
        return;
    }

    if (dest->type == LOC_MEM) {
        if (src->type == LOC_MEM) {
            Location* tmp = start_scratch_reg(src->bytesWidth, out);
            emiti_move(src, tmp, sign_ext, out);
            emiti_move(tmp, dest, sign_ext, out);
            end_scratch_reg(tmp, out);
            return;
        }
    }

    if (src->bytesWidth > dest->bytesWidth) {
        Location* new = loc_opt_copy(src);
        new->bytesWidth = dest->bytesWidth;
        emiti_move(new, dest, sign_ext, out);
        return;
    }

    const char *prefix = "mov ";
    if ((src->type == LOC_MEM && dest->type == LOC_REG && !IsIntReg(dest->v.reg.id)) ||
            (dest->type == LOC_MEM && src->type == LOC_REG && !IsIntReg(src->v.reg.id)))
    {
        if (src->bytesWidth == 4) {
            prefix = "movss ";
        } else if (src->bytesWidth == 8) {
            prefix = "movsd ";
        } else {
            assert(false /* TODO */);
        }
    }
    else if (src->bytesWidth != dest->bytesWidth) {
        if (sign_ext) {
            prefix = "movsx ";
        } else {
            if (dest->bytesWidth == 8 && src->bytesWidth == 4) {
                dest = loc_opt_copy(dest);
                dest->bytesWidth = 4;
            } else {
                prefix = "movzx ";
            }
        }
    }

    fputs(prefix, out);
    emit(dest, out);
    fputs(", ", out);
    emit(src, out);
    fputc('\n', out);
}

static void emiti_cmove(Location* src, Location* dest, const char *cc, FILE* out) {
    assert(src->bytesWidth == dest->bytesWidth);

    fprintf(out, "cmov%s ", cc);
    emit(dest, out);
    fputs(", ", out);
    emit(src, out);
    fputc('\n', out);
}

static void emiti_cmp(Location* a, Location* b, FILE* out);

static void emiti_cmp0(Location* val, FILE* out) {
    if (val->type == LOC_REG || val->type == LOC_MEM) {
        fputs("cmp ", out);
        emit(val, out);
        fputs(", 0\n", out);
    }
    else if (val->type == LOC_IMM || val->type == LOC_EA) {
        Location* scratch = start_scratch_reg(val->bytesWidth, out);
        emiti_move(val, scratch, false, out);
        emiti_cmp0(scratch, out);
        end_scratch_reg(scratch, out);
    }
    else {
        assert(false);
    }
}

static void emiti_cmp(Location* a, Location* b, FILE* out) {
    if (b->bytesWidth != a->bytesWidth) {
        Location* as_size = start_as_size(a->bytesWidth, b, out);
        emiti_cmp(a, as_size, out);
        end_as_size(as_size, b, out);
        return;
    }

    if (b->type == LOC_IMM && b->v.imm.bits == 0) {
        emiti_cmp0(a, out);
        return;
    }

    if (a->type == LOC_IMM || a->type == LOC_EA) {
        Location* scratch = start_scratch_reg(a->bytesWidth, out);
        emiti_move(a, scratch, false, out);
        emiti_cmp(scratch, b, out);
        end_scratch_reg(scratch, out);
        return;
    }

    Location* as_prim_b = start_as_primitive(b, out);

    fputs("cmp ", out);
    emit(a, out);
    fputs(", ", out);
    emit(as_prim_b, out);
    fputc('\n', out);

    end_as_primitive(as_prim_b, b, out);
}

static void emiti_binary(Location* a, Location* b, Location* o, const char * binary, FILE* out) {
    size_t size = o->bytesWidth;

    if (a->type == LOC_MEM && b->type == LOC_MEM) {
        Location* reg_b = start_as_primitive(b, out);
        emiti_binary(a, reg_b, o, binary, out);
        end_as_primitive(reg_b, b, out);
        return;
    }

    if (a->bytesWidth != size) {
        Location* as_size = start_as_size(size, a, out);
        emiti_binary(as_size, b, o, binary, out);
        end_as_size(as_size, a, out);
        return;
    }

    if (b->bytesWidth != size) {
        Location* as_size = start_as_size(size, b, out);
        emiti_binary(a, as_size, o, binary, out);
        end_as_size(as_size, b, out);
        return;
    }

    if (a != o) {
        if (b == o) {
            Location* scratch = start_scratch_reg(size, out);
            emiti_move(a, scratch, false, out);
            emiti_binary(scratch, b, scratch, binary, out);
            emiti_move(scratch, o, false, out);
            end_scratch_reg(scratch, out);
        } else {
            emiti_move(a, o, false, out);
            emiti_binary(o, b, o, binary, out);
        }
        return;
    }

    fprintf(out, "%s ", binary);
    emit(a, out);
    fputs(", ", out);
    emit(b, out);
    fputc('\n', out);
}

static void emiti_unary(Location* v, Location* o, const char * unary, FILE* out) {
    if (v != o) {
        if (o->type == LOC_MEM) {
            Location* scratch = start_scratch_reg(v->bytesWidth, out);
            emiti_move(v, scratch, false, out);
            emiti_unary(scratch, scratch, unary, out);
            emiti_move(scratch, o, false, out);
            end_scratch_reg(scratch, out);
        }
        else {
            emiti_move(v, o, false, out);
            emiti_unary(o, o, unary, out);
        }

        return;
    }

    fprintf(out, "%s ", unary);
    emit(v, out);
    fputc('\n', out);
}

static size_t SCRATCH_REG;
static size_t SCRATCH_REG2;
static size_t XMM_SCRATCH_REG1;
static size_t XMM_SCRATCH_REG2;

typedef struct {
    vx_IrType* type;
    Location* location;
} VarData;

VarData* varData = NULL;

static Location* as_loc(size_t width, vx_IrValue val) {
    switch (val.type) {
        case VX_IR_VAL_VAR:
            return varData[val.var].location;

        case VX_IR_VAL_IMM_INT:
            return gen_imm_var(width, val.imm_int);

        case VX_IR_VAL_IMM_FLT: {
            int64_t bits = *(int64_t*)&val.imm_flt;
            return gen_imm_var(width, bits);
        }

        case VX_IR_VAL_ID: {
            Location* loc = fastalloc(sizeof(Location));
            static char str[16];
            sprintf(str, ".l%zu", val.id);
            *loc = LocLabel(faststrdup(str));
            return loc;
        }

        case VX_IR_VAL_BLOCKREF: {
            Location* loc = fastalloc(sizeof(Location));
            *loc = LocLabel(val.block->name);
            return loc;
        }

        case VX_IR_VAL_UNINIT: {
            Location* loc = fastalloc(sizeof(Location));
            loc->type = LOC_INVALID;
            return loc;
        }

		case VX_IR_VAL_SYMREF: {
			Location* loc = fastalloc(sizeof(Location));
			loc->type = LOC_SYM;
			loc->v.sym.sym = val.symref;
			return loc;
		}

        default:
            assert(false);
            return NULL;
    }
}

static void emit_call_arg_load(vx_IrOp* callOp, FILE* file) {
    assert(callOp->args_len <= 6);

    vx_IrValue fn = *vx_IrOp_param(callOp, VX_IR_NAME_ADDR);

    vx_IrType* type = vx_IrValue_type(cu, callOp->parent, fn);
    assert(type);
    assert(type->kind == VX_IR_TYPE_FUNC);
    vx_IrTypeFunc fnType = type->func;

    char regs[6] = { REG_RDI.id, REG_RSI.id, REG_RDX.id, REG_RCX.id, REG_R8.id, REG_R9.id };

    for (size_t i = 0; i < callOp->args_len; i ++) {
        vx_IrType* type = fnType.args[i];
        size_t size = vx_IrType_size(cu, block, type);
        Location* src = as_loc(size, callOp->args[i]);
        Location* dest = gen_reg_var(size, regs[i]);
        emiti_move(src, dest, false, file);
    }
}

static void emit_call_ret_store(vx_IrOp* callOp, FILE* file) {
    if (callOp->outs_len > 0) {
        vx_IrVar ret = callOp->outs[0].var;
        Location* retl = varData[ret].location;
        Location* eax = gen_reg_var(retl->bytesWidth, REG_RAX.id);
        emiti_move(eax, retl, false, file);
    }
}

static void emit_condmove(vx_IrOp* op, const char *cc, FILE* file) {
    vx_IrValue vthen = *vx_IrOp_param(op, VX_IR_NAME_COND_THEN);
    assert(vx_IrOp_param(op, VX_IR_NAME_COND_ELSE) == NULL);
    Location* out = varData[op->outs[0].var].location;

    emiti_cmove(as_loc(out->bytesWidth, vthen), out, cc, file);
}

static size_t var_used_after_and_before_overwrite(vx_IrOp* after, vx_IrVar var) {
    size_t i = 0;
    for (vx_IrOp *op = after->next; op; op = op->next) {
        for (size_t o = 0; o < op->outs_len; o ++)
            if (op->outs[o].var == var)
                break;

        if (vx_IrOp_varUsed(op, var))
            i ++;
    }
    return i;
}

static bool ops_after_and_before_usage_or_overwrite(vx_IrOp* after, vx_IrVar var) {
    return after->next && !vx_IrOp_varUsed(after->next, var);
}

static void emiti_flt_to_int(Location* src, Location* dest, FILE* file) {
    Location* dest_v = start_as_dest_reg(dest, file);
    if (dest_v->bytesWidth < 4)
        dest_v->bytesWidth = 4;

    const char *op;
    if (src->bytesWidth == 4) {
        op = "cvttss2si ";
    } else if (src->bytesWidth == 8) {
        op = "cvttsd2si ";
    } else {
        assert(false);
    }

    fputs(op, file);
    emit(dest_v, file);
    fputs(", ", file);
    emit(src, file);
    fputc('\n', file);

    end_as_dest_reg(dest_v, dest, file);
}

static void emiti_int_to_flt(Location* src, Location* dest, FILE* file) {
    Location* dest_v = start_as_dest_reg(dest, file);
    Location* src_v = start_as_size(dest->bytesWidth, src, file);

    const char *op;
    if (dest->bytesWidth == 4) {
        op = "cvtsi2ss ";
    } else if (dest->bytesWidth == 8) {
        op = "cvtsi2sd ";
    } else {
        assert(false);
    }

    fputs(op, file);
    emit(dest_v, file);
    fputs(", ", file);
    emit(src_v, file);
    fputc('\n', file);

    end_as_size(src_v, src, file);
    end_as_dest_reg(dest_v, dest, file);
}

static void emiti_bittest(Location* val, Location* idx, FILE* file) {
    if (val->bytesWidth == 1) {
        Location* valv = start_as_size(2, val, file);
        emiti_bittest(valv, idx, file);
        end_as_size(valv, val, file);
        return;
    }

    Location* idxv = start_as_primitive(idx, file);

    fputs("bt ", file);
    emit(val, file);
    fputs(", ", file);
    emit(idx, file);
    fputc('\n', file);

    end_as_primitive(idxv, idx, file);
}

static void emiti_push(Location* l, FILE* file) {
	fputs("push ", file);
	emit(l, file);
	fputc('\n', file);
}

static void emiti_pop(Location* l, FILE* file) {
	fputs("pop ", file);
	emit(l, file);
	fputc('\n', file);
}

static void emiti_moddiv(Location* o, Location* a, Location* b, bool sign, bool ismod, FILE* file) {
	Location* need_pop_rax = NULL;
	if (REG_RAX.stored && REG_RAX.stored != a) {
		need_pop_rax = REG_RAX.stored;
		emiti_push(REG_RAX.stored, file);
	}
	Location arax = LocReg(a->bytesWidth, REG_RAX.id);
	emiti_move(a, &arax, false, file);

	Location* need_pop_rdx = NULL;
	if (REG_RDX.stored && REG_RDX.stored != o) {
		need_pop_rdx = REG_RDX.stored;
		emiti_push(REG_RDX.stored, file);
	}

	if (sign) {
		if (a->bytesWidth == 4) {
			fputs("cdq\n", file);
		} else if (a->bytesWidth == 8) {
			fputs("cqo\n", file);
		} else {
			assert(false && "x86 backend: unsupported operand a int width for mod/div");
		}
	}

	fputs(sign ? "idiv " : "div ", file);
	emit(b, file);
	fputc('\n', file);

	if (ismod) {
		Location oo = LocReg(o->bytesWidth, REG_RDX.id);
		emiti_move(&oo, o, false, file);
	}

	if (need_pop_rdx) {
		emiti_pop(need_pop_rdx, file);
	}

	if (need_pop_rax) {
		emiti_pop(need_pop_rax, file);
	}
}

static void emiti_shift(Location* o, Location* a, Location* b, char const* op, FILE* file) {
	Location* need_pop_rcx = NULL;
	if (REG_RCX.stored && REG_RCX.stored != b) {
		need_pop_rcx = REG_RCX.stored;
		emiti_push(REG_RCX.stored, file);
	}
	Location cl = LocReg(1, REG_RCX.id);
	emiti_move(b, &cl, false, file);

	fprintf(file, "%s ", op);
	emit(a, file);
	fputs(", cl\n", file);

	if (need_pop_rcx) {
		emiti_pop(need_pop_rcx, file);
	}
}

static void emiti_ret(vx_IrBlock* block, vx_IrValue* values, FILE* out) {
    if (block->ll_out_types_len >= 1) {
        VarData vd = varData[values[0].var];
        Location* src = vd.location;

        assert(vd.type != NULL);

        char reg = REG_RAX.id;
        if (vd.type->kind == VX_IR_TYPE_KIND_BASE && vd.type->base.isfloat)
            reg = REG_XMM0.id;

        Location* dest = gen_reg_var(src->bytesWidth, reg);
        emiti_move(src, dest, false, out);
    }

    if (block->ll_out_types_len >= 2) {
        VarData vd = varData[values[1].var];
        Location* src = vd.location;

        assert(vd.type != NULL);

        char reg = REG_RDX.id;
        if (vd.type->kind == VX_IR_TYPE_KIND_BASE && vd.type->base.isfloat)
            reg = REG_XMM1.id;

        Location* dest = gen_reg_var(src->bytesWidth, reg);
        emiti_move(src, dest, false, out);
    }

    assert(block->ll_out_types_len <= 2);

    if (needEpilog)
        emiti_leave(out);
    fputs("ret\n", out);
}

static vx_IrOp* emiti(vx_IrOp *prev, vx_IrOp* op, FILE* file) {
	if (op->backend != NULL) {
		vx_IrOp_x86* x86 = vx_IrOp_x86_get(op);
		if (x86->no_cg) return op->next;
	}

    switch (op->id) {
        case VX_IR_OP_RETURN:
            {
                emiti_ret(block, op->args, file);
            } break;

        case VX_IR_OP_FROMFLT: // "val" 
            {
                vx_IrValue val = *vx_IrOp_param(op, VX_IR_NAME_VALUE);
                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;
                emiti_flt_to_int(as_loc(outLoc->bytesWidth, val), outLoc, file);
            } break;

        case VX_IR_OP_TOFLT: // "val" 
            {
                vx_IrValue val = *vx_IrOp_param(op, VX_IR_NAME_VALUE);
                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;
                emiti_int_to_flt(as_loc(outLoc->bytesWidth, val), outLoc, file);
            } break;

        case VX_IR_OP_FLTCAST: // "val"
            {
                vx_IrValue val = *vx_IrOp_param(op, VX_IR_NAME_VALUE);
                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;

                Location* dest = outLoc;
                if (outLoc->type != LOC_REG) {
                    Location temp; dest = &temp;
                    *dest = LocReg(dest->bytesWidth, XMM_SCRATCH_REG1);
                }

                vx_IrType* ty = vx_IrValue_type(cu, block, val);
                Location* val_loc = as_loc(vx_IrType_size(cu, block, ty), val);

                Location* src = val_loc;
                if (val_loc->type != LOC_REG) {
                    Location temp = LocReg(dest->bytesWidth, XMM_SCRATCH_REG2);
                    emiti_move(src, &temp, false, file);
                    src = &temp;
                }

                // have src & dest now

                if (src->bytesWidth == 8 && dest->bytesWidth == 8) {
                } else if (src->bytesWidth == 8) {
                    assert(dest->bytesWidth == 4);

                    fputs("cvtsd2ss ", file);
                    emit(dest, file);
                    fputs(", ", file);
                    emit(src, file);
                    fputc('\n', file);
                } 
                else {
                    assert(dest->bytesWidth == 8);
                    assert(src->bytesWidth == 4);

                    fputs("cvtss2sd ", file);
                    emit(dest, file);
                    fputs(", ", file);
                    emit(src, file);
                    fputc('\n', file);
                }

                if (outLoc->type != LOC_REG) {
                    emiti_move(dest, outLoc, false, file);
                }
            } break;

        case VX_IR_OP_BITCAST:     // "val"
        case VX_IR_OP_IMM: // "val"
        case VX_IR_OP_ZEROEXT:     // "val"
            {
                vx_IrValue val = *vx_IrOp_param(op, VX_IR_NAME_VALUE);
                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;
                emiti_move(as_loc(outLoc->bytesWidth, val), outLoc, false, file);
            } break;

        case VX_IR_OP_SIGNEXT:     // "val"
            {
                vx_IrValue val = *vx_IrOp_param(op, VX_IR_NAME_VALUE);
                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;
                emiti_move(as_loc(outLoc->bytesWidth, val), outLoc, true, file);
            } break;

        case VX_IR_OP_LOAD:            // "addr"
        case VX_IR_OP_LOAD_VOLATILE:   // "addr"
            {
                vx_IrValue val = *vx_IrOp_param(op, VX_IR_NAME_ADDR);
                Location* val_loc = as_loc(PTRSIZE, val);
                Location* addr_loc = start_as_primitive(val_loc, file);

                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;
                Location* mem = gen_mem_var(outLoc->bytesWidth, addr_loc);
                emiti_move(mem, outLoc, false, file);

                end_as_primitive(addr_loc, val_loc, file);
            } break;

        case VX_IR_OP_STORE:           // "addr", "val"
        case VX_IR_OP_STORE_VOLATILE:  // "addr", "val"
            {
                vx_IrValue addrV = *vx_IrOp_param(op, VX_IR_NAME_ADDR);
                vx_IrValue valV = *vx_IrOp_param(op, VX_IR_NAME_VALUE);
                vx_IrType* type = vx_IrValue_type(cu, block, valV);
                assert(type);

                Location* addr = as_loc(PTRSIZE, addrV);
                Location* addr_real = start_as_primitive(addr, file);

                Location* val = as_loc(vx_IrType_size(cu, block, type), valV);
                Location* mem = gen_mem_var(val->bytesWidth, addr_real);
                emiti_move(val, mem, false, file);

                end_as_primitive(addr_real, addr, file);
            } break;

        case VX_IR_OP_PLACE:           // "var"
            {
                vx_IrValue valV = *vx_IrOp_param(op, VX_IR_NAME_VAR);
                assert(valV.type == VX_IR_VAL_VAR && "inliner fucked up (VX_IR_OP_PLACE)");

                assert(block->as_root.vars[valV.var].ever_placed);

                Location* loc = varData[valV.var].location;
                assert(loc->type == LOC_MEM && "register allocator fucked up (VX_IR_OP_PLACE)");

                vx_IrVar out = op->outs[0].var;
                Location* outLoc = varData[out].location;

                emiti_move(loc->v.mem.address, outLoc, false, file);
            } break;

        case VX_IR_OP_LOAD_EA:         // "ptr", "idx", "elsize"       base + elsize * idx
        case VX_IR_OP_STORE_EA:        // "val", "ptr", "idx", "elsize"       base + elsize * idx
        case VX_IR_OP_EA:              // "ptr", "offset", "idx", "elsize"       base + offset + elsize * idx
            {
                Location* o = op->outs_len > 0 ? varData[op->outs[0].var].location : NULL;

                size_t eaBytesWidth = o ? o->bytesWidth : 0;
                switch (op->id)
                {
                    case VX_IR_OP_LOAD_EA:
                    case VX_IR_OP_STORE_EA:
                        eaBytesWidth = PTRSIZE;
                        break;

                    default: break;
                }

                vx_IrValue* base = vx_IrOp_param(op, VX_IR_NAME_ADDR); 
                Location* base_loc = base ? as_loc(eaBytesWidth, *base) : NULL;

                vx_IrValue* idx = vx_IrOp_param(op, VX_IR_NAME_IDX); 
                Location* idx_loc = idx ? as_loc(eaBytesWidth, *idx) : NULL;

                vx_IrValue* elsize = vx_IrOp_param(op, VX_IR_NAME_ELSIZE); 
                Location* elsize_loc = elsize ? as_loc(eaBytesWidth, *elsize) : NULL;

                size_t numMemOrEa = 0;
                if (base_loc && (base_loc->type == LOC_MEM || base_loc->type == LOC_EA)) numMemOrEa ++;
                if (idx_loc && (idx_loc->type == LOC_MEM || idx_loc->type == LOC_EA)) numMemOrEa ++;
                if (elsize_loc && (elsize_loc->type == LOC_MEM || elsize_loc->type == LOC_EA)) numMemOrEa ++;

                switch (op->id)
                {
                    case VX_IR_OP_LOAD_EA:
                    case VX_IR_OP_STORE_EA:
                        numMemOrEa = 0;
                        break;

                    default: break;
                }

                size_t multiplicationNumNotImm = 0;
                if (idx_loc && idx_loc->type != LOC_IMM) multiplicationNumNotImm ++;
                if (elsize_loc && elsize_loc->type != LOC_IMM) multiplicationNumNotImm ++;

                // branch always taken if loadea or storeea
                if (numMemOrEa < 2 && multiplicationNumNotImm < 2)
                {
                    Location* base_prim_loc = base_loc ? start_as_primitive(base_loc, file) : NULL;
                    Location* idx_prim_loc = idx_loc ? start_as_primitive(idx_loc, file) : NULL;
                    Location* elsize_prim_loc = elsize_loc ? start_as_primitive(elsize_loc, file) : NULL;

                    Location* ea = fastalloc(sizeof(Location));
                    *ea = LocEA(PTRSIZE, base_prim_loc, idx_prim_loc, 1, elsize_prim_loc);

                    switch (op->id)
                    {
                        case VX_IR_OP_LOAD_EA: {
                            assert(o);
                            Location* mem = fastalloc(sizeof(Location));
                            *mem = LocMem(o->bytesWidth, ea);
                            emiti_move(mem, o, false, file);
                            break;
                        }

                        case VX_IR_OP_STORE_EA: {
                            vx_IrType* valTy = vx_IrValue_type(cu, block, *vx_IrOp_param(op, VX_IR_NAME_VALUE));
                            size_t bytes = vx_IrType_size(cu, block, valTy);
                            Location* val = as_loc(bytes, *vx_IrOp_param(op, VX_IR_NAME_VALUE));
                            Location* mem = fastalloc(sizeof(Location));
                            *mem = LocMem(bytes, ea);
                            emiti_move(val, mem, false, file);
                            break;
                        }

                        case VX_IR_OP_EA: {
                            assert(o);
                            emiti_move(ea, o, false, file);
                            break;
                        }

                        default: assert(false); break;
                    }

                    if (elsize_prim_loc) end_as_primitive(elsize_prim_loc, elsize_loc, file);
                    if (idx_prim_loc) end_as_primitive(idx_prim_loc, idx_loc, file);
                    if (base_prim_loc) end_as_primitive(base_prim_loc, base_loc, file);
                }
                else 
                {
                    if (base_loc && idx_loc && elsize_loc)
                    {
                        Location* mult = start_scratch_reg(eaBytesWidth, file);
                        emiti_binary(idx_loc, elsize_loc, mult, "imul", file);
                        emiti_binary(base_loc, mult, o, "add", file);
                        end_scratch_reg(mult, file);
                    }
                    else if (base_loc && idx_loc)
                    {
                        emiti_binary(base_loc, idx_loc, o, "add", file);
                    }
                    else if (idx_loc && elsize_loc)
                    {
                        emiti_binary(idx_loc, elsize_loc, o, "imul", file);
                    }
                    else
                    {
                        assert(false);
                    }
                }
            } break;

        case VX_IR_OP_UDIV: // "a", "b"
        case VX_IR_OP_SDIV: // "a", "b"
        case VX_IR_OP_UMOD: // "a", "b"
        case VX_IR_OP_SMOD: // "a", "b"
			{
                Location* o = varData[op->outs[0].var].location;
                assert(o);
                Location* a = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_A));
                Location* b = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_B));

				bool sign = op->id == VX_IR_OP_SDIV || op->id == VX_IR_OP_SMOD;
				bool mod = op->id == VX_IR_OP_SMOD || op->id == VX_IR_OP_UMOD;
				emiti_moddiv(o, a, b, sign, mod, file);
			} break;

        case VX_IR_OP_SHL: // "a", "b"
        case VX_IR_OP_SHR: // "a", "b"
        case VX_IR_OP_ASHR: // "a", "b"
			{
                Location* o = varData[op->outs[0].var].location;
                assert(o);
                Location* a = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_A));
                Location* b = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_B));

                const char * bin;
                switch (op->id) {
                    case VX_IR_OP_SHL: bin = "shl"; break;
                    case VX_IR_OP_SHR: bin = "shr"; break;
                    case VX_IR_OP_ASHR: bin = "sar"; break;

                    default: assert(false); break;
                }

				emiti_shift(o, a, b, bin, file);
			} break;

        case VX_IR_OP_ADD: // "a", "b"
        case VX_IR_OP_SUB: // "a", "b"
            {
                Location* o = varData[op->outs[0].var].location;
                assert(o);
                Location* a = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_A));
                Location* b = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_B));

                if (!equal(o, a) && a->type == LOC_REG && (op->id == VX_IR_OP_ADD || b->type != LOC_REG)) {
                    int sign = op->id == VX_IR_OP_ADD ? 1 : -1;

                    Location* ea = fastalloc(sizeof(Location));
                    *ea = LocEA(o->bytesWidth, a, b, sign, NULL);
                    emiti_move(ea, o, false, file);

                    break;
                }
            } // no break 

        case VX_IR_OP_MUL: // "a", "b"
        case VX_IR_OP_AND: // "a", "b"
        case VX_IR_OP_BITWISE_AND: // "a", "b"
        case VX_IR_OP_OR:  // "a", "b"
        case VX_IR_OP_BITIWSE_OR:  // "a", "b"
            {
                Location* o = varData[op->outs[0].var].location;
                assert(o);
                Location* a = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_A));
                Location* b = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_B));

                const char * bin;
                switch (op->id) {
                    case VX_IR_OP_ADD: bin = "add"; break;
                    case VX_IR_OP_SUB: bin = "sub"; break;
                    case VX_IR_OP_MUL: bin = "imul"; break;
                    case VX_IR_OP_AND: 
                    case VX_IR_OP_BITWISE_AND: bin = "and"; break;
                    case VX_IR_OP_OR:
                    case VX_IR_OP_BITIWSE_OR: bin = "or"; break;

                    default: assert(false); break;
                }

                emiti_binary(a, b, o, bin, file);
            } break;

        case VX_IR_OP_NOT: // "val"
        case VX_IR_OP_BITWISE_NOT: // "val"
            {
                Location* o = varData[op->outs[0].var].location;
                assert(o);
                Location* v = as_loc(o->bytesWidth, *vx_IrOp_param(op, VX_IR_NAME_VALUE));
                emiti_unary(v, o, "not", file);
            } break;

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
        case VX_IR_OP_BITEXTRACT:
		case VX_IR_OP_COND:
		case VX_IR_OP_CMOV:
			assert(false && "forgot to run x86_llir_conditionals pass?");
			break;

		case VX_IR_OP_X86_BITTEST:
			{
				Location* idx = as_loc(1, *vx_IrOp_param(op, VX_IR_NAME_IDX));
				Location* val = as_loc(PTRSIZE, *vx_IrOp_param(op, VX_IR_NAME_VALUE));
				emiti_bittest(val, idx, file);
			} break;

		case VX_IR_OP_X86_CMP:
            {
				Location* a = as_loc(PTRSIZE, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_A));
				Location* b = as_loc(PTRSIZE, *vx_IrOp_param(op, VX_IR_NAME_OPERAND_B));
				emiti_cmp(a, b, file);
            } break;

		case VX_IR_OP_X86_SETCC:
			{
                vx_IrVar ov = op->outs[0].var;
                Location* o = varData[ov].location;
                assert(o);
				const char * cc = vx_IrOp_param(op, VX_IR_NAME_COND)->x86_cc;
				emiti_storecond(o, cc, file);
			} break;

        case VX_IR_OP_X86_JMPCC:         // "id", "cond": x86_cc
            {
                vx_IrValue id = *vx_IrOp_param(op, VX_IR_NAME_ID);
                vx_IrValue cond = *vx_IrOp_param(op, VX_IR_NAME_COND);
                emiti_jump_cond(as_loc(PTRSIZE, id), cond.x86_cc, file);
            } break;

        case VX_IR_OP_X86_CMOV:          // "cond": x86_cc, "then": value, "else": value
            {
                vx_IrValue cond = *vx_IrOp_param(op, VX_IR_NAME_COND);
                emit_condmove(op, cond.x86_cc, file);
            } break;

        case VX_IR_OP_LABEL:        // "id"
            {
                vx_IrValue id = *vx_IrOp_param(op, VX_IR_NAME_ID);
                fprintf(file, ".l%zu:\n", id.id);
            } break;

        case VX_IR_OP_GOTO:         // "id"
            {
                Location* tg = as_loc(PTRSIZE, *vx_IrOp_param(op, VX_IR_NAME_ID));
                emiti_jump(tg, file);
            } break;

        case VX_IR_OP_CALL:          // "addr": int / fnref
            {
                vx_IrValue addr = *vx_IrOp_param(op, VX_IR_NAME_ADDR);
                emit_call_arg_load(op, file);
                emiti_call(as_loc(PTRSIZE, addr), file);
                emit_call_ret_store(op, file);
            } break;

        case VX_IR_OP_TAILCALL:      // "addr": int / fnref
            {
                assert(!needEpilog);
                vx_IrValue addr = *vx_IrOp_param(op, VX_IR_NAME_ADDR);
                emit_call_arg_load(op, file);
                emiti_jump(as_loc(PTRSIZE, addr), file);
            } break;

        default:
            assert(false);
            break;
    }

    return op->next;
}

void vx_cg_x86stupid_gen(vx_CU* _cu, vx_IrBlock* _block, FILE* out) {
	cu = _cu;
	block = _block;

	fprintf(out, "bits 64\n");
    fprintf(out, "%s:\n", block->name);

    assert(block->is_root);

    bool is_leaf = vx_IrBlock_llIsLeaf(block);
    bool use_rax = is_leaf &&
        block->ll_out_types_len > 0;
    vx_IrType* use_rax_type = NULL;
    vx_OptIrVar optLastRetFirstArg = VX_IRVAR_OPT_NONE;
    if (use_rax) {
        use_rax_type = block->ll_out_types[0];
        assert(use_rax_type);
        if (vx_IrType_size(cu, block, use_rax_type) > 8) {
            use_rax = false;
            use_rax_type = NULL;
        } else {
            vx_IrOp* lastRet = vx_IrBlock_lastOfType(block, VX_IR_OP_RETURN);
            if (lastRet) {
                vx_IrValue val = lastRet->args[0];
                if (val.type == VX_IR_VAL_VAR) {
                    optLastRetFirstArg = VX_IRVAR_OPT_SOME(val.var);
                } else {
                    use_rax = false;
                    use_rax_type = NULL;
                }
            } else {
                use_rax = false;
                use_rax_type = false;
            }
        }
    }

    bool anyPlaced = false;
    for (vx_IrVar var = 0; var < block->as_root.vars_len; var ++) {
        if (block->as_root.vars[var].ever_placed) {
            anyPlaced = true;
        }
    }

    // arguments 1-6 : RDI, RSI, RDX, RCX, R8, R9
    // always used   : RBP, RSP, R10 
    // calle cleanup : RBX, R12, R13, R14, R15

    size_t availableRegistersCount = 1;
    if (use_rax) {
        availableRegistersCount --;
        SCRATCH_REG = REG_R11.id;
    }

    char * availableRegisters = fastalloc(availableRegistersCount);
    if (!use_rax) {
        SCRATCH_REG = REG_RAX.id;
        availableRegisters[0] = REG_R11.id;
    }

    SCRATCH_REG2 = REG_R10.id;

    size_t anyIntArgsCount = block->ins_len;
    size_t anyCalledIntArgsCount = 0;
    size_t anyCalledXmmArgsCount = 0;
    // max arg len 
    for (vx_IrOp* op = block->first; op != NULL; op = op->next) {
        if (op->id == VX_IR_OP_CALL || op->id == VX_IR_OP_TAILCALL) {
            vx_IrValue addr = *vx_IrOp_param(op, VX_IR_NAME_ADDR);
            vx_IrType* ty = vx_IrValue_type(cu, block, addr);
            assert(ty != NULL);
            assert(ty->kind == VX_IR_TYPE_FUNC);

            vx_IrTypeFunc fn = ty->func;

            size_t usedInt = 0;
            size_t usedXmm = 0;

            for (size_t i = 0; i < fn.args_len; i ++) {
                vx_IrType* arg = fn.args[i];
                assert(arg->kind == VX_IR_TYPE_KIND_BASE);

                if (arg->base.isfloat)
                    usedXmm ++;
                else usedInt ++;
            }

            if (usedInt > anyCalledIntArgsCount)
                anyCalledIntArgsCount = usedInt;
            if (usedXmm > anyCalledXmmArgsCount)
                anyCalledXmmArgsCount = usedXmm;
        }
    }
    XMM_SCRATCH_REG1 = anyCalledXmmArgsCount;
    XMM_SCRATCH_REG2 = anyCalledXmmArgsCount + 1;

    if (anyCalledIntArgsCount > anyIntArgsCount)
        anyIntArgsCount = anyCalledIntArgsCount;

    if (anyIntArgsCount < 6) {
        size_t extraAv = 6 - anyIntArgsCount;
        availableRegisters = fastrealloc(availableRegisters, availableRegistersCount, availableRegistersCount + extraAv);

        availableRegisters[availableRegistersCount] = REG_R9.id;
        if (extraAv > 1)
            availableRegisters[availableRegistersCount + 1] = REG_R8.id;
		if (extraAv > 2)
			availableRegisters[availableRegistersCount + 2] = REG_RCX.id;
		if (extraAv > 3)
			availableRegisters[availableRegistersCount + 3] = REG_RDX.id;
		if (extraAv > 4)
			availableRegisters[availableRegistersCount + 4] = REG_RSI.id;
		if (extraAv > 5)
			availableRegisters[availableRegistersCount + 5] = REG_RDI.id;
        availableRegistersCount += extraAv;
    }

    varData = block->as_root.vars_len == 0 ? NULL : calloc(block->as_root.vars_len, sizeof(VarData));
	assert(varData);

    for (vx_IrVar var = 0; var < block->as_root.vars_len; var ++) {
        varData[var].type = block->as_root.vars[var].ll_type;
    }

    size_t stackOff = 0;

    /* ======================== VAR ALLOC ===================== */ 

    vx_IrVar* varsHotFirst = calloc(block->as_root.vars_len, sizeof(vx_IrVar));
	bool* varsSorted = calloc(block->as_root.vars_len, sizeof(bool));
    size_t highestHeat = 0;
	for (vx_IrVar var = 0; var < block->as_root.vars_len; var ++) {
        size_t heat = block->as_root.vars[var].heat;
        if (heat > highestHeat) {
            highestHeat = heat;
        }
    }
	size_t varsHotFirstLen = 0;
	for (; highestHeat > 0 ; highestHeat --) {
		for (vx_IrVar var = 0; var < block->as_root.vars_len; var ++) {
			if (varsSorted[var]) continue;
			size_t heat = block->as_root.vars[var].heat;
			if (heat == 0) continue;
			if (heat == highestHeat) {
				varsHotFirst[varsHotFirstLen++] = var;
				varsSorted[var] = true;
			}
		}
	}
	free(varsSorted);

    size_t varId = 0;
    for (size_t i = 0; i < availableRegistersCount; i ++) {
        char reg = availableRegisters[i];

        if (varId >= varsHotFirstLen) break;

        for (; varId < varsHotFirstLen; varId ++) {
            vx_IrVar var = varsHotFirst[varId];

            vx_IrType* type = varData[var].type;
            if (type == NULL) continue;

            size_t size = vx_IrType_size(cu, block, type);
            if (size > 8) continue;

            if (block->as_root.vars[var].ever_placed) continue; 

            size = widthToWidthWidth(size);
            varData[var].location = gen_reg_var(size, reg);
            break;
        }
        varId ++;
    }

    if (optLastRetFirstArg.present) {
        size_t size = vx_IrType_size(cu, block, varData[optLastRetFirstArg.var].type);
        size = widthToWidthWidth(size);
        varData[optLastRetFirstArg.var].location = gen_reg_var(size, REG_RAX.id);
    }

    char intArgRegs[6] = { REG_RDI.id, REG_RSI.id, REG_RDX.id, REG_RCX.id, REG_R8.id, REG_R9.id };

    size_t id_i = 0;
    size_t id_f = 0;
    struct VarD {
        vx_IrVar var;
        Location* argLoc;
    };
    struct VarD * toMove = NULL;
    size_t toMoveLen = 0;
    for (size_t i = 0; i < block->ins_len; i ++) {
        vx_IrTypedVar var = block->ins[i];
        assert(var.type->kind == VX_IR_TYPE_KIND_BASE);
        size_t size = vx_IrType_size(cu, block, var.type);
        assert(size != 0);

        bool move_into_stack = block->as_root.vars[var.var].ever_placed;
        move_into_stack = move_into_stack && (var.type->base.isfloat ? id_f < 8 : id_i < 6);
        if (move_into_stack) {
            size = widthToWidthWidth(size);
            Location* src;
            if (var.type->base.isfloat) {
                src = gen_reg_var(size, IntRegCount + id_f);
            } else {
                src = gen_reg_var(size, intArgRegs[id_i]);
            }

            Location* loc = gen_stack_var(size, stackOff);
            stackOff += size;
            anyPlaced = true;

            // opposite from cases below
            toMove = realloc(toMove, sizeof(struct VarD) * (toMoveLen + 1));
            toMove[toMoveLen ++] = (struct VarD) {
                .var = var.var,
                .argLoc = src,
            };
            varData[var.var].location = loc;
        }
        else if (var.type->base.isfloat) {
            Location* loc;
            if (id_f >= 8) {
                loc = gen_stack_var(size, stackOff);
                stackOff += size;
                anyPlaced = true;
            } else {
                size = widthToWidthWidth(size);
                loc = gen_reg_var(size, IntRegCount + id_f);
            }

            if (id_f >= anyCalledXmmArgsCount) {
                varData[var.var].location = loc;
            } else {
                toMove = realloc(toMove, sizeof(struct VarD) * (toMoveLen + 1));
                toMove[toMoveLen ++] = (struct VarD) {
                    .var = var.var,
                    .argLoc = loc
                };
            }
        }
        else {
            Location* loc;
            if (id_i >= 6) {
                loc = gen_stack_var(size, stackOff);
                stackOff += size;
                anyPlaced = true;
            } else {
                size = widthToWidthWidth(size);
                loc = gen_reg_var(size, intArgRegs[id_i]);
            }

            if (id_i >= anyCalledIntArgsCount) {
                varData[var.var].location = loc;
            } else {
                toMove = realloc(toMove, sizeof(struct VarD) * (toMoveLen + 1));
                toMove[toMoveLen ++] = (struct VarD) {
                    .var = var.var,
                    .argLoc = loc
                };
            } 
        }

        if (var.type->base.isfloat)
            id_f ++;
        else 
            id_i ++;
    }

    free(varsHotFirst);

    for (vx_IrVar var = 0; var < block->as_root.vars_len; var ++) {
        if (varData[var].location) continue;
        if (varData[var].type == NULL) continue;
		if (block->as_root.vars[var].heat == 0) continue;

        size_t size = vx_IrType_size(cu, block, varData[var].type);
        varData[var].location = gen_stack_var(size, stackOff);
        stackOff += size;
        anyPlaced = true;
    }

    bool needProlog = (stackOff > 0 && !is_leaf) ||
        stackOff > 128 ||
        (stackOff > 0 && cu->target.flags.x86[vx_Target_X86__NO_REDZONE]);

    needEpilog = false;
    if (anyPlaced || needProlog) {
        emiti_enter(out);
        needEpilog = true;
    }

    vx_IrBlock_ll_finalize(cu, block, needEpilog);

    if (needProlog) {
        if (is_leaf && cu->target.flags.x86[vx_Target_X86__NO_REDZONE]) {
            size_t v = 128 - stackOff;
            fprintf(out, "sub rsp, %zu\n", v);
        } else {
            fprintf(out, "sub rsp, %zu\n", stackOff);
        }
    }

    for (size_t i = 0; i < toMoveLen; i ++) {
        Location* dst = varData[toMove[i].var].location;
        Location* src = toMove[i].argLoc;
        emiti_move(src, dst, false, out);
    }
    free(toMove);

    vx_IrOp* op = block->first;
    vx_IrOp* prev = NULL;

    while (op != NULL) {
        vx_IrOp* new = emiti(prev, op, out);
        prev = op;
        op = new;
    }

    free(varData);
    varData = NULL;

    fputc('\n', out);
}

static Location* start_scratch_reg(size_t size, FILE* out) {
    if (size > 8) { // TODO 
        if (RegLut[REG_XMM8.id]->stored) {
            assert(RegLut[REG_XMM9.id]->stored == NULL);
            Location* loc = gen_reg_var(size, REG_XMM9.id);
            RegLut[REG_XMM9.id]->stored = loc;
            return loc;
        }

        Location* loc = gen_reg_var(size, REG_XMM8.id);
        RegLut[REG_XMM8.id]->stored = loc;
        return loc;
    }

    if (RegLut[SCRATCH_REG]->stored == NULL) {
        Location* loc = gen_reg_var(size, SCRATCH_REG);
        RegLut[SCRATCH_REG]->stored = loc;
        return loc;
    } else if (RegLut[SCRATCH_REG2]->stored == NULL) {
        Location* loc = gen_reg_var(size, SCRATCH_REG2);
        RegLut[SCRATCH_REG2]->stored = loc;
        return loc;
    } else {
        assert(/* out of regs */ false);
        return NULL;
    }
}

static void end_scratch_reg(Location* loc, FILE* out) {
    assert(loc->type == LOC_REG);

    char regId = loc->v.reg.id;
    assert(RegLut[(int) regId]->stored == loc);
    RegLut[(int) regId]->stored = NULL;

    loc->type = LOC_INVALID;
}

static Location* start_as_dest_reg(Location* other, FILE* out) {
    if (other->type == LOC_MEM) {
        return start_scratch_reg(other->bytesWidth, out);
    } else {
        assert(other->type == LOC_REG);
    }

    return other;
}

static void end_as_dest_reg(Location* as_reg, Location* old, FILE* out) {
    if (old->type == LOC_MEM) {
        emiti_move(as_reg, old, false, out);
        end_scratch_reg(as_reg, out);
    }
}

static Location* start_as_primitive(Location* other, FILE* out) {
    assert(other->type != LOC_INVALID);

    if (other->type == LOC_EA) {
        Location* s = start_scratch_reg(other->bytesWidth, out);
        emiti_lea(other, s, out);
        return s;
    }

    if (other->type == LOC_MEM) {
        Location* s = start_scratch_reg(other->bytesWidth, out);
        emiti_move(other, s, false, out);
        return s;
    }

    return other;
}

static void end_as_primitive(Location* as_prim, Location* old, FILE* out) {
    if (old->type == LOC_EA || old->type == LOC_MEM) {
        end_scratch_reg(as_prim, out);
    }
}

static Location* start_as_size(size_t size, Location* loc, FILE* out) {
    if (loc->bytesWidth == size) {
        return loc;
    }

    if ((loc->type == LOC_REG && loc->bytesWidth > size) || loc->type == LOC_MEM || loc->type == LOC_IMM || loc->type == LOC_EA) {
        Location* copy = loc_opt_copy(loc);
        copy->bytesWidth = size;
        return copy;
    }

    Location* prim = start_scratch_reg(size, out);
    emiti_move(prim, loc, false, out);

    return prim;
}

static void end_as_size(Location* as_size, Location* old, FILE* out) {
    if (old->bytesWidth == as_size->bytesWidth) return;
    if ((old ->type == LOC_REG && old->bytesWidth > as_size->bytesWidth) || old->type == LOC_MEM || old->type == LOC_IMM || old->type == LOC_EA) return;

    assert(as_size->type == LOC_REG);
    end_scratch_reg(as_size, out);
}
