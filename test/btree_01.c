#include <stdio.h>
#include "btree.h"
#include <stdio.h>

typedef union {
	VAL_T v;
	btree_t t;
} btree_val_t;

struct btree_s {
	size_t innerp:1;
	size_t n:63;
	KEY_T key[63U + 1U/*spare*/];
	btree_val_t val[64U];
	btree_t next;
};


static void
btree_prnt(btree_t t, size_t lvl)
{
	printf("%p\tL%zu", t, lvl);
	if (t->innerp) {
		for (size_t i = 0U; i < t->n; i++) {
			printf("\t%f", (double)t->key[i]);
		}
	} else {
		/* leaves */
		for (size_t i = 0U; i < t->n; i++) {
			printf("\t%f|%f", (double)t->key[i], (double)t->val[i].v);
		}
	}
	putchar('\n');
	if (t->innerp) {
		for (size_t i = 0U; i <= t->n; i++) {
			btree_prnt(t->val[i].t, lvl + 1U);
		}
	}
	return;
}

int
main(void)
{
	btree_t x;

	x = make_btree();
	btree_add(x, 1.23228df, 0.5dd);
	btree_add(x, 1.23226df, 0.5dd);
	btree_add(x, 1.23225df, 0.5dd);
	btree_put(x, 1.23226df, 1.5dd);
	btree_add(x, 1.23230df, 0.5dd);
	btree_add(x, 1.23225df, -0.5dd);
	btree_add(x, 1.23229df, 0.5dd);
	btree_add(x, 1.23232df, 0.5dd);
	btree_add(x, 1.23227df, 0.5dd);
	btree_add(x, 1.23232df, -0.5dd);

	btree_prnt(x, 0U);

	VAL_T v;
	KEY_T k;
	k = btree_min(x, &v);
	printf("min %f (%f)\n", (double)k, (double)v);

	k = btree_max(x, &v);
	printf("max %f (%f)\n", (double)k, (double)v);

	free_btree(x);
	return 0;
}
