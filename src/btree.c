/*** btree.c -- simple b+tree impl
 *
 * Copyright (C) 2016-2017 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of books.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#if defined HAVE_DFP754_H
# include <dfp754.h>
#endif	/* HAVE_DFP754_H */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "btree.h"
#include "nifty.h"

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
root_split(btree_t root)
{
	/* root got split, bollocks */
	const btree_t left = make_btree();
	const btree_t rght = make_btree();
	const size_t piv = countof(root->key) / 2U - 1U;

	/* T will become the new root so push stuff to LEFT ... */
	memcpy(left->key, root->key, (piv + 1U) * sizeof(*root->key));
	memcpy(left->val, root->val, (piv + 1U) * sizeof(*root->val));
	left->innerp = root->innerp;
	left->n = piv + !root->innerp;
	left->next = rght;
	/* ... and RGHT */
	memcpy(rght->key, root->key + piv + 1U, (piv + 0U) * sizeof(*root->key));
	memcpy(rght->val, root->val + piv + 1U, (piv + 1U) * sizeof(*root->val));
	rght->innerp = root->innerp;
	rght->n = piv;
	rght->next = NULL;
	/* and now massage T */
	root->key[0U] = root->key[piv];
	memset(root->key + 1U, -1, sizeof(root->key) - sizeof(*root->key));
	root->val[0U].t = left;
	root->val[1U].t = rght;
	root->n = 1U;
	root->innerp = 1U;
	root->next = NULL;
	return;
}

static void
node_split(btree_t prnt, size_t idx)
{
/* PRNT's IDX-th child will be split */
	const btree_t chld = prnt->val[idx].t;
	const btree_t rght = make_btree();
	const size_t piv = countof(chld->key) / 2U - 1U;

	/* shift things to RGHT */
	memcpy(rght->key, chld->key + piv + 1U, (piv + 0U) * sizeof(*chld->key));
	memcpy(rght->val, chld->val + piv + 1U, (piv + 1U) * sizeof(*chld->val));
	rght->innerp = chld->innerp;
	rght->n = piv;
	rght->next = NULL;

	if (idx < prnt->n) {
		/* make some room then */
		memmove(prnt->key + idx + 1U,
			prnt->key + idx + 0U,
			(countof(prnt->key) - (idx + 1U)) * sizeof(*prnt->key));
		memmove(prnt->val + idx + 1U,
			prnt->val + idx + 0U,
			(countof(prnt->key) - (idx + 1U)) * sizeof(*prnt->val));
	}
	/* and now massage LEFT which is C and T */
	prnt->key[idx] = chld->key[piv];
	prnt->n++;
	prnt->val[idx + 1U].t = rght;
	chld->n = piv + !chld->innerp;
	memset(chld->key + chld->n, -1,
	       (countof(chld->key) - chld->n) * sizeof(*chld->key));
	chld->next = rght;
	return;
}

static bool
leaf_add(btree_t t, KEY_T k, VAL_T *v[static 1U])
{
	size_t nul;
	size_t i;

	for (i = 0U; i < t->n && k > t->key[i]; i++);
	/* so k is <= t->key[i] or t->key[i] is nan */

	if (k == t->key[i]) {
		/* got him */
		goto out;
	}
	/* otherwise do a scan to see if we have spare items */
	for (nul = 0U; nul < t->n && t->val[nul].v > 0.dd; nul++);

	if (nul > i) {
		/* spare item is far to the right */
		memmove(t->key + i + 1U,
			t->key + i + 0U,
			(nul - i) * sizeof(*t->key));
		memmove(t->val + i + 1U,
			t->val + i + 0U,
			(nul - i) * sizeof(*t->val));
	} else if (nul < i) {
		/* spare item to the left, good job
		 * go down with the index as the hole will be to our left */
		i--;
		memmove(t->key + nul + 0U,
			t->key + nul + 1U,
			(i - nul) * sizeof(*t->key));
		memmove(t->val + nul + 0U,
			t->val + nul + 1U,
			(i - nul) * sizeof(*t->val));
	}
	t->n += !(nul < t->n);
	t->key[i] = k;
	t->val[i].v = 0.dd;
out:
	*v = &t->val[i].v;
	return t->n >= countof(t->key) - 1U;
}

static bool
twig_add(btree_t t, KEY_T k, VAL_T *v[static 1U])
{
	bool splitp;
	btree_t c;
	size_t i;

	for (i = 0U; i < t->n && k > t->key[i]; i++);
	/* so k is <= t->key[i] or t->key[i] is nan */

	/* descent */
	c = t->val[i].t;

	if (!c->innerp) {
		/* oh, we're in the leaves again */
		splitp = leaf_add(c, k, v);
	} else {
		/* got to go deeper, isn't it? */
		splitp = twig_add(c, k, v);
	}

	if (splitp) {
		/* C needs splitting, not again */
		node_split(t, i);
	}
	return t->n >= countof(t->key) - 1U;
}


btree_t
make_btree(void)
{
	btree_t r = calloc(1U, sizeof(*r));

	memset(r->key, -1, sizeof(r->key));
	return r;
}

void
free_btree(btree_t t)
{
	if (t->innerp) {
		/* descend and free */
		for (size_t i = 0U; i <= t->n; i++) {
			/* descend */
			free_btree(t->val[i].t);
		}
	}
	free(t);
	return;
}

VAL_T
btree_add(btree_t t, KEY_T k, VAL_T v)
{
	VAL_T *vp;
	bool splitp;

	/* check if root has leaves */
	if (!t->innerp) {
		splitp = leaf_add(t, k, &vp);
	} else {
		splitp = twig_add(t, k, &vp);
	}
	/* do the maths */
	v += *vp;
	/* be saturating */
	v = v > 0.dd ? v : 0.dd;
	*vp = v;

	if (UNLIKELY(splitp)) {
		/* root got split, bollocks */
		root_split(t);
	}
	return v;
}

VAL_T
btree_put(btree_t t, KEY_T k, VAL_T v)
{
	VAL_T *vp;
	bool splitp;

	/* check if root has leaves */
	if (!t->innerp) {
		splitp = leaf_add(t, k, &vp);
	} else {
		splitp = twig_add(t, k, &vp);
	}
	/* be saturating */
	v = v > 0.dd ? v : 0.dd;
	*vp = v;

	if (UNLIKELY(splitp)) {
		/* root got split, bollocks */
		root_split(t);
	}
	return v;
}

KEY_T
btree_min(btree_t t, VAL_T *v)
{
	for (; t->innerp; t = t->val->t);
	do {
		size_t i;
		for (i = 0U; i < t->n; i++) {
			if (LIKELY(t->val[i].v > 0.dd)) {
				if (LIKELY(v != NULL)) {
					*v = t->val[i].v;
				}
				return t->key[i];
			}
		}
	} while ((t = t->next));
	return nand32("");
}

KEY_T
btree_max(btree_t t, VAL_T *v)
{
	KEY_T best_max = nand32("");
	VAL_T best_val;

	for (; t->innerp; t = t->val->t);
	do {
		size_t i;
		for (i = 0U; i < t->n; i++) {
			if (LIKELY(t->val[i].v > 0.dd)) {
				best_max = t->key[i];
				best_val = t->val[i].v;
			}
		}
	} while ((t = t->next));
	if (LIKELY(v != NULL && !isnand32(best_max))) {
		*v = best_val;
	}
	return best_max;
}

/* btree.c ends here */
