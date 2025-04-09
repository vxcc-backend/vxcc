#include "targets_internal.h"
#include "../config.h"

int vx_Target_parse(vx_Target* dest, const char * str)
{
	dest->heap_whole = strdup(str);

    bool found = false;
    size_t strLen = strlen(str);
	for (vx_TargetArch t = 0; t < vx_TargetArch__len; t ++)
    {
        const char * name = vx_TargetArch__entries[t].name.a;
        size_t nameLen = strlen(name);
        if (strLen < nameLen) break;
        if (!memcmp(str, name, nameLen) && (str[nameLen] == ':' || str[nameLen] == '\0'))
        {
            found = true;
            dest->arch = t;
            str += nameLen;
            if (str[0] == ':') str ++;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Could not find target arch\n");
        return 1;
    }

    switch (dest->arch)
    {
        case vx_TargetArch_X86_64:
#if VXCC_BACKEND_X86
            vx_Target_X86__parseAdditionalFlags(&dest->flags.x86, str);
#else
            assert(false && "x86 backend disabled");
#endif
			break;

        case vx_TargetArch_ETCA:
#if VXCC_BACKEND_ETCA
            vx_Target_ETCA__parseAdditionalFlags(&dest->flags.etca, str);
#else
            assert(false && "etca backend disabled");
#endif
			break;

        // add target
    }

    return 0;
}

void vx_Target_info(vx_TargetInfo* dest, vx_Target const* target)
{

    memset(dest, 0, sizeof(vx_TargetInfo));
    switch (target->arch)
    {
        case vx_TargetArch_X86_64:
#if VXCC_BACKEND_X86
        	vx_Target_X86__info(dest, target);
#else
            assert(false && "x86 backend disabled");
#endif
			break;

        case vx_TargetArch_ETCA:
#if VXCC_BACKEND_ETCA
            vx_Target_ETCA__info(dest, target);
#else
            assert(false && "etca backend disabled");
#endif
			break;

        // add target
    }
}

#define TARGET_FLAGS_GEN(tg) \
typedef bool vx_Target_##tg##__flags [vx_Target_##tg##__len]; \
void vx_Target_##tg##__parseAdditionalFlag(vx_Target_##tg##__flags * dest, const char * flag) { \
    assert(flag); \
    for (size_t i = 0; i < vx_Target_##tg##__len; i ++) { \
        if (*dest[i]) continue; \
        vx_Target_##tg##__entry* entry = &vx_Target_##tg##__entries[i]; \
        if (entry->id.a && !strcmp(entry->id.a, flag)) { \
            (*dest)[i] = true; \
            if (entry->infer.set) \
                vx_Target_##tg##__parseAdditionalFlags(dest, entry->infer.a); \
            return; \
        } \
    } \
	fprintf(stderr, "arch flag \"%s\" not found\n", flag); \
} \
void vx_Target_##tg##__parseAdditionalFlags(vx_Target_##tg##__flags * dest, const char * c) { \
    while (c) { \
        size_t len = strchr(c, ',') ? (strchr(c, ',') - c) : strlen(c); \
        char copy[32] = {0}; \
        if (len > 31) \
            len = 31; \
        memcpy(copy, c, len); \
        vx_Target_##tg##__parseAdditionalFlag(dest, copy); \
        c = strchr(c, ','); \
		if (c) c ++; \
    } \
}

TARGET_FLAGS_GEN(ETCA)
TARGET_FLAGS_GEN(X86)
// add target

#undef TARGET_FLAGS_GEN

#include "x86/x86.h"
// add target

void vx_llir_emit_asm(vx_CU* cu, vx_IrBlock* llirblock, FILE* out)
{
	switch (cu->target.arch)
    {
        case vx_TargetArch_X86_64:
#if VXCC_BACKEND_X86
			vx_llir_x86(cu, llirblock);
        	vx_cg_x86stupid_gen(cu, llirblock, out);
#else
            assert(false && "x86 backend disabled");
#endif
			break;

        case vx_TargetArch_ETCA:
#if VXCC_BACKEND_ETCA
            assert(false && "codegen for etca not yet implemented");
#else
            assert(false && "etca backend disabled");
#endif
			break;

        // add target
    }
}

