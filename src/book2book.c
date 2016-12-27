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
	qx_t o;
} quo_t;

#define NOT_A_QUO	(quo_t){SIDE_UNK}
#define NOT_A_QUO_P(x)	!((x).s)

/* output mode */
static void(*prq)(quo_t);
static unsigned int unxp;


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

/* per-run variables */
static const char *prfx;
static size_t prfz;

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

static quo_t
rdq(const char *line, size_t llen)
{
/* process one line */
	char *on;
	quo_t q;

	/* get qty */
	if (UNLIKELY((on = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without quantity */
		return NOT_A_QUO;
	}
	llen = on - line;
	q.q = strtoqx(on + 1U, NULL);

	/* get prc */
	if (UNLIKELY((on = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without price */
		return NOT_A_QUO;
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

		if (UNLIKELY(!q.s)) {
			/* cannot put entry to either side, just ignore */
			return NOT_A_QUO;
		}
	}
	llen = on - line;

	/* see if we've got pairs */
	if (conx) {
		const char *boi =
			memrchr(line, '\t', llen - 1U) ?: deconst(line - 1U);
		hx_t hx = hash(boi + 1U, on - 1U - (boi + 1U));

		if (UNLIKELY(hx != conx)) {
			return NOT_A_QUO;
		}
	}
	/* let them know where the prefix ends */
	prfx = line;
	prfz = llen;
	return q;
}

static quo_t
adq(quo_t q)
{
	switch (q.f) {
		qx_t tmp;
	case LVL_3:
		tmp = btree_add(BOOK(q.s), q.p, q.q);
		q.o = tmp - q.q;
		q.q = tmp;
		break;
	case LVL_2:
		q.o = btree_put(BOOK(q.s), q.p, q.q);
		break;
	case LVL_1:
		/* we'd have to pop anything more top-level in the books ... */
		q.o = btree_put(BOOK(q.s), q.p, q.q);
		break;
	case LVL_0:
	default:
		/* we don't know what to do */
		return NOT_A_QUO;
	}
	return q;
}

static void
prq1(quo_t UNUSED(q))
{
/* convert to 1-books, aligned */
	btree_iter_t bi, ai;

	do {
		static px_t b1, a1;
		static qx_t B1, A1;
		char buf[256U];
		size_t len = 0U;

		bi = (btree_iter_t){.t = BIDS};
		ai = (btree_iter_t){.t = ASKS};

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
		if (unxp && ai.k <= bi.k && ai.k < a1) {
			/* invalidate bid and start again */
			btree_put(BIDS, bi.k, 0.dd);
		} else if (unxp && bi.k >= ai.k && bi.k > b1) {
			/* invalidate ask and start again */
			btree_put(ASKS, ai.k, 0.dd);
		} else {
			/* yep, top level change */
			b1 = bi.k, B1 = bi.v;
			a1 = ai.k, A1 = ai.v;
		}
		len += pxtostr(buf + len, sizeof(buf) - len, bi.k);
		buf[len++] = '\t';
		len += pxtostr(buf + len, sizeof(buf) - len, ai.k);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, bi.v);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, ai.v);
		buf[len++] = '\n';

		fwrite(prfx, 1, prfz, stdout);
		fwrite(buf, 1, len, stdout);
	} while (unxp && ai.k <= bi.k);
	return;
}

static void
prq2(quo_t q)
{
/* print 2-books */
	char buf[256U];
	size_t len = 0U;

	buf[len++] = (char)(q.s ^ '@');
	buf[len++] = '2';
	buf[len++] = '\t';
	len += pxtostr(buf + len, sizeof(buf) - len, q.p);
	buf[len++] = '\t';
	len += qxtostr(buf + len, sizeof(buf) - len, q.q);
	buf[len++] = '\n';

	fwrite(prfx, 1, prfz, stdout);
	fwrite(buf, 1, len, stdout);
	return;
}

static void
prq3(quo_t q)
{
/* convert to 3-books */
	char buf[256U];
	size_t len = 0U;

	buf[len++] = (char)(q.s ^ '@');
	buf[len++] = '3';
	buf[len++] = '\t';
	len += pxtostr(buf + len, sizeof(buf) - len, q.p);
	buf[len++] = '\t';
	len += qxtostr(buf + len, sizeof(buf) - len, q.q - q.o);
	buf[len++] = '\n';

	fwrite(prfx, 1, prfz, stdout);
	fwrite(buf, 1, len, stdout);
	return;
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

	prq = prq2;
	if (argi->dash1_flag) {
		prq = prq1;
	}
	if (argi->dash3_flag) {
		prq = prq3;
	}
	if (argi->dash2_flag) {
		prq = prq2;
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
			quo_t q;

			if (NOT_A_QUO_P(q = rdq(line, nrd))) {
				;
			} else if (NOT_A_QUO_P(q = adq(q))) {
				;
			} else {
				/* print line prefix */
				prq(q);
			}
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
