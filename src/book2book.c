/*** book2book.c -- book converter
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
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#undef _GNU_SOURCE
#if defined HAVE_DFP754_H
# include <dfp754.h>
#endif	/* HAVE_DFP754_H */
#include "dfp754_d32.h"
#include "dfp754_d64.h"
#include "hash.h"
#include "btree.h"
#include "nifty.h"

typedef _Decimal32 px_t;
typedef _Decimal64 qx_t;
#define strtopx		strtod32
#define pxtostr		d32tostr
#define strtoqx		strtod64
#define qxtostr		d64tostr

typedef enum {
	SIDE_UNK,
	SIDE_ASK,
	SIDE_BID,
} side_t;

typedef enum {
	LVL_0,
	LVL_1,
	LVL_2,
	LVL_3,
} flav_t;

typedef struct {
	side_t s;
	flav_t f;
	px_t p;
	qx_t q;
} quo_t;


static inline size_t
memncpy(char *restrict tgt, const char *src, size_t zrc)
{
	(void)memcpy(tgt, src, zrc);
	return zrc;
}


static const char *cont;
static size_t conz;
static hx_t conx;

static btree_t book[2U];
#define BOOK(s)		book[(s) - 1U]
#define BIDS		BOOK(SIDE_BID)
#define ASKS		BOOK(SIDE_ASK)

static void
init(void)
{
	BIDS = make_btree(true);
	ASKS = make_btree(false);
	return;
}

static void
fini(void)
{
#if 0
	{
		btree_iter_t i;
		char buf[16384U];
		size_t len;

		len = memncpy(buf, "BIDS", 4U);
		for (i.t = BIDS; btree_iter_next(&i);) {
			buf[len++] = ' ';
			buf[len++] = ' ';
			len += pxtostr(buf + len, sizeof(buf) - len, i.k);
			buf[len++] = '|';
			len += qxtostr(buf + len, sizeof(buf) - len, i.v);
		}
		buf[len++] = '\n';
		fwrite(buf, 1, len, stdout);

		len = memncpy(buf, "ASKS", 4U);
		for (i.t = ASKS; btree_iter_next(&i);) {
			buf[len++] = ' ';
			buf[len++] = ' ';
			len += pxtostr(buf + len, sizeof(buf) - len, i.k);
			buf[len++] = '|';
			len += qxtostr(buf + len, sizeof(buf) - len, i.v);
		}
		buf[len++] = '\n';
		fwrite(buf, 1, len, stdout);
	}
#endif

	btree_chck(BIDS);
	btree_chck(ASKS);

	free_btree(BIDS);
	free_btree(ASKS);
	return;
}

static int
procln(const char *line, size_t llen)
{
/* process one line */
	char *on;
	quo_t q;

	/* get qty */
	if (UNLIKELY((on = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without quantity */
		return -1;
	}
	llen = on - line;
	q.q = strtoqx(on + 1U, NULL);

	/* get prc */
	if (UNLIKELY((on = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without price */
		return -1;
	}
	llen = on - line;
	q.p = strtopx(on + 1U, NULL);

	/* get flavour, should be just before ON */
	with (unsigned char f = *(unsigned char*)--on) {
		/* map 1, 2, 3 to LVL_{1,2,3}
		 * everything else goes to LVL_0 */
		f ^= '0';
		q.f = (flav_t)(f & -(f < 4U));
	}

	/* rewind manually */
	for (; on > line && on[-1] != '\t'; on--);
	with (unsigned char s = *(unsigned char*)on) {
		/* map A or a to ASK and B or b to BID
		 * everything else goes to SIDE_UNK */
		s &= ~0x20U;
		s ^= '@';
		q.s = (side_t)(s & -(s <= 2U));
	}
	llen = on - line;

	/* see if we've got pairs */
	if (conx) {
		const char *boi =
			memrchr(line, '\t', llen - 1U) ?: deconst(line - 1U);
		hx_t hx = hash(boi + 1U, on - 1U - (boi + 1U));

		if (UNLIKELY(hx != conx)) {
			return -1;
		}
	}

#if 0
/* convert to 2-books */
	if (LIKELY(q.s && q.f)) {
		/* add to book */
		qx_t x = btree_add(BOOK(q.s), q.p, q.q);
		char buf[256U];
		size_t len = 0U;

		buf[len++] = (char)(q.s ^ '@');
		buf[len++] = '2';
		buf[len++] = '\t';
		len += pxtostr(buf + len, sizeof(buf) - len, q.p);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, x);
		buf[len++] = '\n';

		fwrite(line, 1, llen, stdout);
		fwrite(buf, 1, len, stdout);
	}
#elif 1
/* convert to 1-books, aligned */
	if (LIKELY(q.s && q.f)) {
		/* add to book */
		(void)btree_add(BOOK(q.s), q.p, q.q);
	}

again:
	with (btree_iter_t bi = {.t = BIDS}, ai = {.t = ASKS}) {
		static px_t b1, a1;
		static qx_t B1, A1;
		char buf[256U];
		size_t len = 0U;

		/* get the top of the book values */
		if (!btree_iter_next(&bi)) {
			break;
		} else if (!btree_iter_next(&ai)) {
			break;
		} else if ((bi.k == b1 && bi.v == B1 &&
			    ai.k == a1 && ai.v == A1)) {
			break;
		}

		/* check self-crossing */
		if (ai.k <= bi.k && ai.k < a1) {
			/* invalidate bid and start again */
			btree_put(BIDS, bi.k, 0.dd);
			goto again;
		} else if (bi.k >= ai.k && bi.k > b1) {
			/* invalidate ask and start again */
			btree_put(ASKS, ai.k, 0.dd);
			goto again;
		}

		/* yep, top level change */
		b1 = bi.k, B1 = bi.v;
		a1 = ai.k, A1 = ai.v;
		len += pxtostr(buf + len, sizeof(buf) - len, b1);
		buf[len++] = '\t';
		len += pxtostr(buf + len, sizeof(buf) - len, a1);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, B1);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, A1);
		buf[len++] = '\n';

		fwrite(line, 1, llen, stdout);
		fwrite(buf, 1, len, stdout);
	}
#endif
	return 0;
}


#include "book2book.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	int rc = EXIT_SUCCESS;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = EXIT_FAILURE;
		goto out;
	}

	if (argi->instr_arg) {
		cont = argi->instr_arg;
		conz = strlen(cont);
		conx = hash(cont, conz);
	}

	/* initialise the processor */
	init();
	{
		char *line = NULL;
		size_t llen = 0UL;

		for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
			procln(line, nrd);
		}
		free(line);
	}
	/* finalise the processor */
	fini();

out:
	yuck_free(argi);
	return rc;
}

/* book2book.c ends here */
