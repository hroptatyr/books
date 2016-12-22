#include <stdio.h>
#include "btree.h"
#include <stdio.h>


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

	btree_prnt(x);

	VAL_T v;
	KEY_T k;
	k = btree_top(x, &v);
	printf("top %f (%f)\n", (double)k, (double)v);

	k = btree_bot(x, &v);
	printf("bot %f (%f)\n", (double)k, (double)v);

	free_btree(x);
	return 0;
}
