#ifndef _SEXPR_H
#define _SEXPR_H

#include <stdio.h>

enum SNodeType {
  S_LIST,
  S_STRING,
  S_SYMBOL,
  S_INTEGER,
  S_FLOAT
};

typedef struct SNode SNode;
struct SNode {
  SNode *next;
  enum SNodeType type;
  union {
    SNode *list;
    char *value;
  };
};

/** does NOT close file */
SNode *snode_parse(FILE *fp);

void snode_free(SNode *node);

void snode_print(SNode *node, FILE* out);
SNode* snode_expect(SNode *node, enum SNodeType ty);
SNode* snode_geti(SNode* node, size_t i);
SNode* snode_geti_expect(SNode* node, size_t i);

/** takes something like `(name "alex") (pass 123)`; note that next() is used on @list */
SNode* snode_kv_get(SNode* list, char const * key);

/** takes something like `(name "alex") (pass 123)`; note that next() is used on @list */
SNode* snode_kv_get_expect(SNode* list, char const * key);

/** 1 + how many next nodes there are */
size_t snode_num_nodes(SNode* sn);

SNode* snode_mk(enum SNodeType type, char const* value);
SNode* snode_mk_list(SNode* inner);

/** creates a (k v) */
SNode* snode_mk_kv(char const* key, SNode* val);

SNode* snode_tail(SNode* nd);

/** concatenates b to the tail of a and returns the beginning */
SNode* snode_cat(SNode* opta, SNode* b);

/** creates a (a b c d) */
#define snode_mk_listx(arr, arrlen, serial, serialarg) ({\
	SNode* li = NULL; \
	for (size_t i = 0; i < arrlen; i ++) { \
		li = snode_cat(li, serial((serialarg), arr[i])); \
	} \
	snode_mk_list(li); \
})

/** creates a (a b c d) */
#define snode_mk_listxli(begin, next, serial, serialarg) ({\
	SNode* li = NULL; \
	for (typeof(begin) i = (begin); i; i = (i->next)) { \
		li = snode_cat(li, serial((serialarg), i)); \
	} \
	snode_mk_list(li); \
})

#endif
