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
#include "books.h"
#include "nifty.h"

#define strtopx		strtod32
#define pxtostr		d32tostr
#define strtoqx		strtod64
#define qxtostr		d64tostr

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

static book_t book;
//#define BOOK(s)		book[(s) - 1U]
//#define BIDS		BOOK(SIDE_BID)
//#define ASKS		BOOK(SIDE_ASK)

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
	book = make_book();

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

	book = free_book(book);
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
		q.f = /*(flav_t)*/(f & -(f < 4U));
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

static void
prq1(quo_t UNUSED(q))
{
/* convert to 1-books, aligned */
	quo_t b, a;
	do {
		static quo_t oldb, olda;
		char buf[256U];
		size_t len = 0U;

		b = book_top(book, SIDE_BID);
		a = book_top(book, SIDE_ASK);

		if ((b.p == oldb.p && b.q == oldb.q &&
		     a.p == olda.p && a.q == olda.q)) {
			break;
		}

		/* uncross */
		if (0) {
			;
		} else if (unxp && a.p <= b.p && a.p < olda.p) {
			/* remove bid */
			book_add(book, (quo_t){SIDE_BID, LVL_2, b.p, 0.dd});
		} else if (unxp && b.p >= a.p && b.p > oldb.p) {
			/* remove ask */
			book_add(book, (quo_t){SIDE_ASK, LVL_2, a.p, 0.dd});
		} else {
			/* yep, top level change */
			oldb = b;
			olda = a;
		}

		buf[len++] = 'c';
		buf[len++] = '1';
		buf[len++] = '\t';
		len += pxtostr(buf + len, sizeof(buf) - len, b.p);
		buf[len++] = '\t';
		len += pxtostr(buf + len, sizeof(buf) - len, a.p);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, b.q);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, a.q);
		buf[len++] = '\n';

		fwrite(prfx, 1, prfz, stdout);
		fwrite(buf, 1, len, stdout);
	} while (unxp && a.p <= b.p);
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
		size_t bn = book_tops(b, B, book, SIDE_BID, ntop);
		size_t an = book_tops(a, A, book, SIDE_ASK, ntop);

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

		size_t n = ntop < bn && ntop < an ? ntop : bn < an ? an : bn;
		for (size_t i = 0U; i < n; i++) {
			char buf[256U];
			size_t len = 0U;

			len += snprintf(buf + len, sizeof(buf) - len,
					"c%zu", i + 1U);
			buf[len++] = '\t';
			if (i < bn) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, b[i]);
			}
			buf[len++] = '\t';
			if (i < an) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, a[i]);
			}
			buf[len++] = '\t';
			if (i < bn) {
				len += qxtostr(
					buf + len, sizeof(buf) - len, B[i]);
			}
			buf[len++] = '\t';
			if (i < an) {
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
	quo_t bc, ac;

	do {
		static quo_t oldb, olda;
		char buf[256U];
		size_t len = 0U;

		bc = book_ctop(book, SIDE_BID, cqty);
		ac = book_ctop(book, SIDE_ASK, cqty);

		if (bc.p == oldb.p && ac.p == olda.p) {
			break;
		}

		/* check self-crossing */
		if (unxp) {
			;
		}

		/* assign to state vars already */
		oldb = bc, olda = ac;

		buf[len++] = 'C';
		len += qxtostr(buf + len, sizeof(buf) - len, cqty);
		buf[len++] = '\t';
		if (bc.q >= cqty) {
			len += pxtostr(buf + len, sizeof(buf) - len, bc.p);
		}
		buf[len++] = '\t';
		if (ac.q >= cqty) {
			len += pxtostr(buf + len, sizeof(buf) - len, ac.p);
		}
		buf[len++] = '\t';
		if (bc.q >= cqty) {
			len += qxtostr(buf + len, sizeof(buf) - len, bc.q);
		}
		buf[len++] = '\t';
		if (ac.q >= cqty) {
			len += qxtostr(buf + len, sizeof(buf) - len, ac.q);
		}
		buf[len++] = '\n';

		fwrite(prfx, 1, prfz, stdout);
		fwrite(buf, 1, len, stdout);
	} while (unxp && ac.p <= bc.p);
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

	memset(b, 0, sizeof(b));
	memset(B, 0, sizeof(B));
	memset(a, 0, sizeof(a));
	memset(A, 0, sizeof(A));

	do {
		size_t bn = book_ctops(b, B, book, SIDE_BID, cqty, ntop);
		size_t an = book_ctops(a, A, book, SIDE_ASK, cqty, ntop);

		if (!memcmp(b, bids, sizeof(b)) &&
		    !memcmp(a, asks, sizeof(a))) {
			/* nothing's changed, sod off */
			return;
		}

		if (unxp) {
			/* uncrossing code here */
			;
		}

		qx_t eoc = cqty;
		size_t n = ntop < bn && ntop < an ? ntop : bn < an ? an : bn;
		for (size_t i = 0U; i < n; i++, eoc += cqty) {
			char buf[256U];
			size_t len = 0U;

			buf[len++] = 'C';
			len += qxtostr(buf + len, sizeof(buf) - len, eoc);
			buf[len++] = '\t';
			if (i < bn) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, b[i]);
			}
			buf[len++] = '\t';
			if (i < an) {
				len += pxtostr(
					buf + len, sizeof(buf) - len, a[i]);
			}
			buf[len++] = '\t';
			if (i < bn) {
				len += qxtostr(
					buf + len, sizeof(buf) - len, B[i]);
			}
			buf[len++] = '\t';
			if (i < an) {
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
				/* invalid quote line */
				continue;
			}
			/* we have to unwind second levels manually
			 * because we need to print the interim steps */
			if (UNLIKELY(q.f == LVL_1 &&
				     (prq == prq2 || prq == prq3))) {
				book_iter_t i = book_iter(book, q.s);
				while (book_iter_next(&i) &&
				       (q.s == SIDE_BID && i.p > q.p ||
					q.s == SIDE_ASK && i.p < q.p)) {
					quo_t r = {
						q.s, LVL_2,
						.p = i.p,
						.q = 0.dd
					};
					r = book_add(book, r);
					prq(r);
				}
			}
			/* add to book */
			q = book_add(book, q);
			if (LIKELY(q.o == q.q)) {
				/* nothing changed */
				continue;
			}
			/* printx */
			prq(q);
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
