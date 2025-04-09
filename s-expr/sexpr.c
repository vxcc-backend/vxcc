#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sexpr.h"

#define BUFFER_MAX 512

static int is_float(char *str) {
  char *ptr = NULL;
  strtod(str, &ptr);
  return !*ptr;
}

static int is_integer(char *str) {
  char *ptr = NULL;
  strtol(str, &ptr, 10);
  return !*ptr;
}

static int is_lst_term(int c) {
  return c == EOF || isspace(c) || c == '(' || c == ')';
}

static int is_str_term(int c) {
  return c == EOF || c == '"';
}

static char *read_value(FILE *fp, int *c, int (*is_term)(int)) {
  int len = 0;
  char buffer[BUFFER_MAX + 1];

  while (!is_term(*c = fgetc(fp)) && len < BUFFER_MAX) {
    buffer[len] = *c;
    len++;
  }
  buffer[len] = '\0';
  
  char *str = malloc((len + 1) * sizeof(char));
  return strcpy(str, buffer);
}

// Recursively parse an s-expression from a file stream
SNode *snode_parse(FILE *fp) {
  // Using a linked list, nodes are appended to the list tail until we 
  // reach a list terminator at which point we return the list head.
  SNode *tail, *head = NULL;
  int c;

  while ((c = fgetc(fp)) != EOF) {
    SNode *node = NULL;

    if (c == ')') {
      // Terminate list recursion
      break;
    } else if (c == '(') {
      // Begin list recursion
      node = malloc(sizeof(SNode));
      node->type = S_LIST;
      node->list = snode_parse(fp);
    } else if (c == '"') {
      // Read a string
      node = malloc(sizeof(SNode));
      node->type = S_STRING;
      node->value = read_value(fp, &c, &is_str_term);
    } else if (!isspace(c)) {
      // Read a float, integer, or symbol
      ungetc(c, fp);
      
      node = malloc(sizeof(SNode));
      node->value = read_value(fp, &c, &is_lst_term);

      // Put the terminator back
      ungetc(c, fp);

      if (is_integer(node->value)) {
        node->type = S_INTEGER;
      } else if (is_float(node->value)) {
        node->type = S_FLOAT;
      } else {
        node->type = S_SYMBOL;
      }
    }

    if (node != NULL) {
      // Terminate the node
      node->next = NULL;
      
      if (head == NULL) {
        // Initialize the list head
        head = tail = node;
      } else {
        // Append the node to the list tail
        tail = tail->next = node;
      }
    }
  }

  return head;
}

// Recursively free memory allocated by a node
void snode_free(SNode *node) {
  while (node != NULL) {
    SNode *tmp = node;

    if (node->type == S_LIST) {
      snode_free(node->list);
    } else {
      // Free current value
      free(node->value);
      node->value = NULL;
    }

    node = node->next;

    // Free current node
    free(tmp);
    tmp = NULL;
  }
}

void snode_print(SNode *node, FILE* out)
{
	while (node)
	{
		switch (node->type)
		{
			case S_FLOAT:
			case S_SYMBOL:
			case S_INTEGER:
				fprintf(out, "%s", node->value);
				break;

			case S_STRING:
				fprintf(out, "\"%s\"", node->value);
				break;

			case S_LIST:
				fprintf(out, "(");
				snode_print(node->list, out);
				fprintf(out, ")");
				break;
		}
		node = node->next;
		if (node)
			fprintf(out, " ");
	}
}

SNode* snode_expect(SNode *node, enum SNodeType ty)
{
	static char const * to_str[] = {
		[S_FLOAT] = "float",
		[S_LIST]  = "list",
		[S_SYMBOL] = "symbol",
		[S_STRING] = "string",
		[S_INTEGER] = "int",
	};

	if (node->type != ty) {
		fprintf(stderr, "expected S node to be of type %s, but got %s\n", to_str[node->type], to_str[ty]);
		exit(1);
	}

	return node;
}

SNode* snode_geti(SNode* node, size_t i)
{
	for (; node && i > 0; i --)
	{
		node = node->next;
	}
	return node;
}

SNode* snode_geti_expect(SNode* node, size_t i)
{
	node = snode_geti(node, i);
	if (!node) {
		fprintf(stderr, "need list to be at least %zu elements long\n", i+1);
		exit(1);
	}
	return node;
}

/** takes something like `(name "alex") (pass 123)`; note that next() is used on @list */
SNode* snode_kv_get(SNode* list, char const * key)
{
	while (list) 
	{
		if (list->type != S_LIST) {
			fprintf(stderr, "that's not how maps work\n");
			exit(1);
		}

		SNode* kv = list->list;
		SNode* k = snode_geti_expect(kv, 0);
		SNode* v = snode_geti_expect(kv, 1);

		snode_expect(k, S_SYMBOL);
        assert(k->value);
        assert(key);
		if (!strcmp(k->value, key))
			return v;

		list = list->next;
	}

	return NULL;
}

/** takes something like `(name "alex") (pass 123)`; note that next() is used on @list */
SNode* snode_kv_get_expect(SNode* list, char const * key)
{
	list = snode_kv_get(list, key);
	if (!list) {
		fprintf(stderr, "need key %s in kv map\n", key);
		exit(1);
	}
	return list;
}

/** 1 + how many next nodes there are */
size_t snode_num_nodes(SNode* sn)
{
	size_t n = 0;
	while (sn) {
		n ++;
		sn = sn->next;
	}
	return n;
}

SNode* snode_mk(enum SNodeType type, char const* value)
{
	SNode* nd = malloc(sizeof(SNode));
	nd->type = type;
	nd->next = NULL;
	nd->value = strdup(value);
	return nd;
}

SNode* snode_mk_list(SNode* inner)
{
	SNode* nd = malloc(sizeof(SNode));
	nd->type = S_LIST;
	nd->next = NULL;
	nd->list = inner;
	return nd;
}

/** creates a (k v) */
SNode* snode_mk_kv(char const* key, SNode* val)
{
	SNode* nd = snode_mk(S_SYMBOL, key);
	nd->next = val;
	return snode_mk_list(nd);
}

SNode* snode_tail(SNode* nd)
{
	if (!nd) return NULL;
	for (; nd->next; nd = nd->next);
	return nd;
}

/** concatenates b to the tail of a and returns the beginning */
SNode* snode_cat(SNode* opta, SNode* b)
{
	if (!opta) return b;
	snode_tail(opta)->next = b;
	return opta;
}
