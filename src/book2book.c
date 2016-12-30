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
#include <stdarg.h>
#include <errno.h>
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


static __attribute__((format(printf, 1, 2))) void
serror(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
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

/* for N-books */
static size_t ntop;
static qx_t cqty;
static px_t *bids;
static px_t *asks;
static qx_t *bszs;
static qx_t *aszs;

static void
init(void)
{
	BIDS = make_btree(true);
	ASKS = make_btree(false);

	if (ntop > 1U) {
		bids = calloc(ntop, sizeof(*bids));
		asks = calloc(ntop, sizeof(*asks));
		bszs = calloc(ntop, sizeof(*bszs));
		aszs = calloc(ntop, sizeof(*aszs));
	}
	return;
}

static void
fini(void)
{
	if (ntop > 1U) {
		free(bids);
		free(asks);
		free(bszs);
		free(aszs);
	}

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

static __attribute__((noinline)) quo_t
adq(quo_t q)
{
	switch (q.f) {
		qx_t tmp;
		btree_iter_t i;
	case LVL_3:
		tmp = btree_add(BOOK(q.s), q.p, q.q);
		q.o = tmp - q.q;
		q.q = tmp;
		break;
	case LVL_2:
		q.o = btree_put(BOOK(q.s), q.p, q.q);
		break;
	case LVL_1:
		if (UNLIKELY(q.q <= 0.dd)) {
			/* what an odd level-1 quote */
			return NOT_A_QUO;
		}
		/* we'd have to pop anything more top-level in the books ...
		 * we put the value first so it's guaranteed to be in there */
		q.o = btree_put(BOOK(q.s), q.p, q.q);
		/* now iter away anything from top that isn't our quote */
		i = (btree_iter_t){.t = BOOK(q.s)};
		while (btree_iter_next(&i) && i.k != q.p) {
			tmp = btree_put(BOOK(q.s), i.k, 0.dd);
			prq((quo_t){q.s, q.f, i.k, 0.dd, tmp});
		}
		break;
	case LVL_0:
	default:
		/* we don't know what to do */
		return NOT_A_QUO;
	}
	if (UNLIKELY(q.q == q.o)) {
		/* we're not repeating stuff */
		return NOT_A_QUO;
	}
	prq(q);
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
		buf[len++] = 'c';
		buf[len++] = '1';
		buf[len++] = '\t';
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

static void
prqn(quo_t UNUSED(q))
{
/* convert to n-books, aligned */
	px_t b[ntop];
	qx_t B[ntop];
	px_t a[ntop];
	qx_t A[ntop];

	memset(b, 0, sizeof(b));
	memset(B, 0, sizeof(B));
	memset(a, 0, sizeof(a));
	memset(A, 0, sizeof(A));

	do {
		btree_iter_t bi = {.t = BIDS}, ai = {.t = ASKS};

		/* get the top of the book values */
		for (size_t i = 0U; i < ntop && btree_iter_next(&bi); i++) {
			b[i] = bi.k;
			B[i] = bi.v;
		}
		for (size_t i = 0U; i < ntop && btree_iter_next(&ai); i++) {
			a[i] = ai.k;
			A[i] = ai.v;
		}
		if (!memcmp(B, bszs, sizeof(B)) &&
		    !memcmp(A, aszs, sizeof(A)) &&
		    !memcmp(b, bids, sizeof(b)) &&
		    !memcmp(a, asks, sizeof(a))) {
			/* nothing's changed, sod off */
			return;
		}

		if (unxp) {
			/* uncrossing code here */
			;
		}

		for (size_t i = 0U; i < ntop; i++) {
			char buf[256U];
			size_t len = 0U;

			len += snprintf(buf + len, sizeof(buf) - len,
					"c%zu", i + 1U);
			buf[len++] = '\t';
			if (B[i]) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, b[i]);
			}
			buf[len++] = '\t';
			if (A[i]) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, a[i]);
			}
			buf[len++] = '\t';
			if (B[i]) {
				len += qxtostr(
					buf + len, sizeof(buf) - len, B[i]);
			}
			buf[len++] = '\t';
			if (A[i]) {
				len += qxtostr(
					buf + len, sizeof(buf) - len, A[i]);
			}
			buf[len++] = '\n';

			fwrite(prfx, 1, prfz, stdout);
			fwrite(buf, 1, len, stdout);
		}
	} while (unxp && a[0U] <= b[0U]);

	memcpy(bids, b, sizeof(b));
	memcpy(asks, a, sizeof(a));
	memcpy(bszs, B, sizeof(B));
	memcpy(aszs, A, sizeof(A));
	return;
}

static void
prqc(quo_t UNUSED(q))
{
/* convert to consolidated 1-books, aligned */
	static px_t b1, a1;
	qx_t bc = 0.dd, ac = 0.dd;
	qx_t Bc = 0.dd, Ac = 0.dd;

	do {
		btree_iter_t bi = {.t = BIDS}, ai = {.t = ASKS};
		char buf[256U];
		size_t len = 0U;

		for (; btree_iter_next(&bi) && Bc < cqty;
		     bc += bi.k * bi.v, Bc += bi.v);
		for (; btree_iter_next(&ai) && Ac < cqty;
		     ac += ai.k * ai.v, Ac += ai.v);
		/* get prices */
		bc /= Bc;
		ac /= Ac;

		if ((px_t)bc == b1 && (px_t)ac == a1) {
			break;
		}

		/* check self-crossing */
		if (unxp) {
			;
		}

		/* assign to state vars already */
		b1 = (px_t)bc, a1 = (px_t)ac;

		buf[len++] = 'C';
		len += qxtostr(buf + len, sizeof(buf) - len, cqty);
		buf[len++] = '\t';
		if (Bc >= cqty) {
			len += pxtostr(buf + len, sizeof(buf) - len, b1);
		}
		buf[len++] = '\t';
		if (Ac >= cqty) {
			len += pxtostr(buf + len, sizeof(buf) - len, a1);
		}
		buf[len++] = '\t';
		if (Bc >= cqty) {
			len += qxtostr(buf + len, sizeof(buf) - len, Bc);
		}
		buf[len++] = '\t';
		if (Ac >= cqty) {
			len += qxtostr(buf + len, sizeof(buf) - len, Ac);
		}
		buf[len++] = '\n';

		fwrite(prfx, 1, prfz, stdout);
		fwrite(buf, 1, len, stdout);
	} while (unxp && ac <= bc);
	return;
}

static void
prqCn(quo_t UNUSED(q))
{
/* convert to n-books, aligned */
	px_t b[ntop];
	qx_t B[ntop];
	px_t a[ntop];
	qx_t A[ntop];

	do {
		btree_iter_t bi = {.t = BIDS}, ai = {.t = ASKS};
		qx_t c, C;
		qx_t eoc;

		/* get the top of the book values */
		eoc = cqty;
		c = C = 0.dd;
		for (size_t i = 0U; i < ntop; i++, eoc += cqty) {
			for (; btree_iter_next(&bi) && C < eoc;
			     c += bi.k * bi.v, C += bi.v);
			b[i] = (px_t)(c / C);
			B[i] = C;
		}
		eoc = cqty;
		c = C = 0.dd;
		for (size_t i = 0U; i < ntop; i++, eoc += cqty) {
			for (;
			     btree_iter_next(&ai) && C < eoc;
			     c += ai.k * ai.v, C += ai.v);
			a[i] = (px_t)(c / C);
			A[i] = C;
		}
		if (!memcmp(b, bids, sizeof(b)) &&
		    !memcmp(a, asks, sizeof(a))) {
			/* nothing's changed, sod off */
			return;
		}

		if (unxp) {
			/* uncrossing code here */
			;
		}

		eoc = cqty;
		for (size_t i = 0U; i < ntop; i++, eoc += cqty) {
			char buf[256U];
			size_t len = 0U;

			buf[len++] = 'C';
			len += qxtostr(buf + len, sizeof(buf) - len, eoc);
			buf[len++] = '\t';
			if (B[i] >= eoc) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, b[i]);
			}
			buf[len++] = '\t';
			if (A[i] >= eoc) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, a[i]);
			}
			buf[len++] = '\t';
			if (B[i] >= eoc) {
				len += qxtostr(
					buf + len, sizeof(buf) - len, B[i]);
			}
			buf[len++] = '\t';
			if (A[i] >= eoc) {
				len += qxtostr(
					buf + len, sizeof(buf) - len, A[i]);
			}
			buf[len++] = '\n';

			fwrite(prfx, 1, prfz, stdout);
			fwrite(buf, 1, len, stdout);
		}
	} while (unxp && a[0U] <= b[0U]);

	memcpy(bids, b, sizeof(b));
	memcpy(asks, a, sizeof(a));
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

	if (argi->dashN_arg) {
		if (!(ntop = strtoul(argi->dashN_arg, NULL, 10))) {
			errno = 0, serror("\
Error: cannot read number of levels for top-N book");
			rc = EXIT_FAILURE;
			goto out;
		}
		if (ntop > 1U) {
			prq = prqn;
		} else {
			prq = prq1;
		}
	}

	if (argi->dashC_arg) {
		if ((cqty = strtoqx(argi->dashC_arg, NULL)) <= 0.dd) {
			errno = 0, serror("\
Error: cannot read consolidated quantity");
			rc = EXIT_FAILURE;
			goto out;
		}
		if (ntop > 1U) {
			prq = prqCn;
		} else {
			prq = prqc;
		}
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
			} else {
				/* add to book and print */
				adq(q);
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
