/*** book2book.c -- book converter
 *
 * Copyright (C) 2016-2018 Sebastian Freundt
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
#include "xquo.h"
#include "nifty.h"

#define strtopx		strtod64
#define pxtostr		d64tostr
#define strtoqx		strtod64
#define qxtostr		d64tostr

typedef struct {
	book_t book;
	union {
		struct {
			px_t *bids;
			px_t *asks;
			qx_t *bszs;
			qx_t *aszs;
		};
		struct {
			px_t bid;
			px_t ask;
			qx_t bsz;
			qx_t asz;
		};
	};
} xbook_t;

#define HX_CATCHALL	((hx_t)-1ULL)

/* output mode */
static void(*prq)(xbook_t*, book_quo_t, book_quo_t);
/* for N-books */
static size_t ntop;
/* consolidation, either quantity or value (price*quantity) */
static qx_t cqty;


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

static xbook_t
make_xbook(void)
{
	xbook_t r = {make_book()};
	if (ntop > 1U) {
		r.bids = calloc(ntop, sizeof(*r.bids));
		r.asks = calloc(ntop, sizeof(*r.asks));
		r.bszs = calloc(ntop, sizeof(*r.bszs));
		r.aszs = calloc(ntop, sizeof(*r.aszs));
	}
	return r;
}

static xbook_t
free_xbook(xbook_t xb)
{
	if (ntop > 1U) {
		free(xb.bids);
		free(xb.asks);
		free(xb.bszs);
		free(xb.aszs);
	}
	xb.book = free_book(xb.book);
	return xb;
}


/* per-run variables */
static const char *prfx;
static size_t prfz;

static void
prq1(xbook_t *xb, book_quo_t UNUSED(q), book_quo_t UNUSED(o))
{
/* convert to 1-books, aligned */
	book_quo_t b, a;
	char buf[256U];
	size_t len = 0U;

	b = book_top(xb->book, BOOK_SIDE_BID);
	a = book_top(xb->book, BOOK_SIDE_ASK);

	if ((b.p == xb->bid && b.q == xb->bsz &&
	     a.p == xb->ask && a.q == xb->asz)) {
		return;
	}
	/* yep, top level change */
	xb->bid = b.p;
	xb->ask = a.p;
	xb->bsz = b.q;
	xb->asz = a.q;

	buf[len++] = 'c';
	buf[len++] = '1';
	buf[len++] = '\t';
	if (b.q) {
		len += pxtostr(buf + len, sizeof(buf) - len, b.p);
	}
	buf[len++] = '\t';
	if (a.q) {
		len += pxtostr(buf + len, sizeof(buf) - len, a.p);
	}
	buf[len++] = '\t';
	if (b.q) {
		len += qxtostr(buf + len, sizeof(buf) - len, b.q);
	}
	buf[len++] = '\t';
	if (a.q) {
		len += qxtostr(buf + len, sizeof(buf) - len, a.q);
	}
	buf[len++] = '\n';

	fwrite(prfx, 1, prfz, stdout);
	fwrite(buf, 1, len, stdout);
	return;
}

static void
prq2(xbook_t *UNUSED(xb), book_quo_t q, book_quo_t UNUSED(o))
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
prq3(xbook_t *UNUSED(xb), book_quo_t q, book_quo_t o)
{
/* convert to 3-books */
	char buf[256U];
	size_t len = 0U;

	buf[len++] = (char)(q.s ^ '@');
	buf[len++] = '3';
	buf[len++] = '\t';
	len += pxtostr(buf + len, sizeof(buf) - len, q.p);
	buf[len++] = '\t';
	len += qxtostr(buf + len, sizeof(buf) - len, q.q - o.q);
	buf[len++] = '\n';

	fwrite(prfx, 1, prfz, stdout);
	fwrite(buf, 1, len, stdout);
	return;
}

static void
prqn(xbook_t *xb, book_quo_t UNUSED(q), book_quo_t UNUSED(o))
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

	size_t bn = book_tops(b, B, xb->book, BOOK_SIDE_BID, ntop);
	size_t an = book_tops(a, A, xb->book, BOOK_SIDE_ASK, ntop);

	if (!memcmp(B, xb->bszs, sizeof(B)) &&
	    !memcmp(A, xb->aszs, sizeof(A)) &&
	    !memcmp(b, xb->bids, sizeof(b)) &&
	    !memcmp(a, xb->asks, sizeof(a))) {
		/* nothing's changed, sod off */
		return;
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

	/* keep a copy for next time */
	memcpy(xb->bids, b, sizeof(b));
	memcpy(xb->asks, a, sizeof(a));
	memcpy(xb->bszs, B, sizeof(B));
	memcpy(xb->aszs, A, sizeof(A));
	return;
}

static void
prqc(xbook_t *xb, book_quo_t UNUSED(q), book_quo_t UNUSED(o))
{
/* convert to consolidated 1-books, aligned */
	book_quo_t bc, ac;
	char buf[256U];
	size_t len = 0U;

	bc = book_ctop(xb->book, BOOK_SIDE_BID, cqty);
	ac = book_ctop(xb->book, BOOK_SIDE_ASK, cqty);

	if (bc.p == xb->bid && ac.p == xb->ask) {
		return;
	}

	/* assign to state vars already */
	xb->bid = bc.p, xb->ask = ac.p;

	buf[len++] = 'c';
	buf[len++] = '1';
	buf[len++] = '\t';
	if (bc.q) {
		len += pxtostr(buf + len, sizeof(buf) - len, bc.p);
	}
	buf[len++] = '\t';
	if (ac.q) {
		len += pxtostr(buf + len, sizeof(buf) - len, ac.p);
	}
	buf[len++] = '\t';
	if (bc.q) {
		len += qxtostr(buf + len, sizeof(buf) - len, bc.q);
	}
	buf[len++] = '\t';
	if (ac.q) {
		len += qxtostr(buf + len, sizeof(buf) - len, ac.q);
	}
	buf[len++] = '\n';

	fwrite(prfx, 1, prfz, stdout);
	fwrite(buf, 1, len, stdout);
	return;
}

static void
prqcn(xbook_t *xb, book_quo_t UNUSED(q), book_quo_t UNUSED(o))
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

	size_t bn = book_ctops(b, B, xb->book, BOOK_SIDE_BID, cqty, ntop);
	size_t an = book_ctops(a, A, xb->book, BOOK_SIDE_ASK, cqty, ntop);

	if (!memcmp(b, xb->bids, sizeof(b)) &&
	    !memcmp(a, xb->asks, sizeof(a))) {
		/* nothing's changed, sod off */
		return;
	}

	qx_t eoc = cqty;
	size_t n = ntop < bn && ntop < an ? ntop : bn < an ? an : bn;
	for (size_t i = 0U; i < n; i++, eoc += cqty) {
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

	memcpy(xb->bids, b, sizeof(b));
	memcpy(xb->asks, a, sizeof(a));
	return;
}

static void
prqv(xbook_t *xb, book_quo_t UNUSED(q), book_quo_t UNUSED(o))
{
/* convert to value-consolidated 1-books, aligned */
	book_quo_t bc, ac;
	char buf[256U];
	size_t len = 0U;

	bc = book_vtop(xb->book, BOOK_SIDE_BID, cqty);
	ac = book_vtop(xb->book, BOOK_SIDE_ASK, cqty);

	if (bc.p == xb->bid && ac.p == xb->ask) {
		return;
	}

	/* assign to state vars already */
	xb->bid = bc.p, xb->ask = ac.p;

	buf[len++] = 'c';
	buf[len++] = '1';
	buf[len++] = '\t';
	if (bc.q) {
		len += pxtostr(buf + len, sizeof(buf) - len, bc.p);
	}
	buf[len++] = '\t';
	if (ac.q) {
		len += pxtostr(buf + len, sizeof(buf) - len, ac.p);
	}
	buf[len++] = '\t';
	if (bc.q) {
		len += qxtostr(buf + len, sizeof(buf) - len, bc.q);
	}
	buf[len++] = '\t';
	if (ac.q) {
		len += qxtostr(buf + len, sizeof(buf) - len, ac.q);
	}
	buf[len++] = '\n';

	fwrite(prfx, 1, prfz, stdout);
	fwrite(buf, 1, len, stdout);
	return;
}

static void
prqvn(xbook_t *xb, book_quo_t UNUSED(q), book_quo_t UNUSED(o))
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

	size_t bn = book_vtops(b, B, xb->book, BOOK_SIDE_BID, cqty, ntop);
	size_t an = book_vtops(a, A, xb->book, BOOK_SIDE_ASK, cqty, ntop);

	if (!memcmp(b, xb->bids, sizeof(b)) &&
	    !memcmp(a, xb->asks, sizeof(a))) {
		/* nothing's changed, sod off */
		return;
	}

	qx_t eoc = cqty;
	size_t n = ntop < bn && ntop < an ? ntop : bn < an ? an : bn;
	for (size_t i = 0U; i < n; i++, eoc += cqty) {
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

	memcpy(xb->bids, b, sizeof(b));
	memcpy(xb->asks, a, sizeof(a));
	return;
}


#include "book2book.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	int rc = EXIT_SUCCESS;
	static hx_t *conx;
	static xbook_t *book;
	static size_t nbook;
	static size_t zbook;
	size_t nctch = 0U;

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
		const char *s = argi->dashC_arg;

		if (*s != '/') {
			/* quantity consolidation */
			if (ntop > 1U) {
				prq = prqcn;
			} else {
				prq = prqc;
			}
		} else {
			/* value consolidation */
			if (ntop > 1U) {
				prq = prqvn;
			} else {
				prq = prqv;
			}
		}
		/* advance S if value consolidation */
		s += *s == '/';

		if ((cqty = strtoqx(s, NULL)) <= 0.dd) {
			errno = 0, serror("\
Error: cannot read consolidated quantity");
			rc = EXIT_FAILURE;
			goto out;
		}
	}

	if ((nbook = argi->instr_nargs)) {
		const char *const *cont = argi->instr_args;
		size_t j = 0U;

		/* initialise hash array and books */
		conx = malloc(nbook * sizeof(*conx));
		book = malloc(nbook * sizeof(*book));

		for (size_t i = 0U; i < nbook; i++) {
			const size_t conz = strlen(cont[i]);

			if (UNLIKELY(conz == 0U ||
				     conz == 1U && *cont[i] == '*')) {
				/* catch-all hash */
				continue;
			}
			conx[j] = hash(cont[i], strlen(cont[i]));
			book[j] = make_xbook();
			j++;
		}
		if (j < nbook) {
			/* oh, there's been catch-alls,
			 * shrink NBOOK and initialise last cell */
			nbook = j;
			conx[nbook] = HX_CATCHALL;
			book[nbook] = make_xbook();
			nctch = 1U;
		}
	} else {
		/* allocate some 8U books */
		zbook = 8U;
		conx = malloc(zbook * sizeof(*conx));
		book = malloc(zbook * sizeof(*book));
	}

	/* initialise the processor */
	{
		char *line = NULL;
		size_t llen = 0UL;

		for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
			xquo_t q;
			size_t k;
			book_quo_t o;
			hx_t hx;

			if (NOT_A_XQUO_P(q = read_xquo(line, nrd))) {
				/* invalid quote line */
				continue;
			}
			/* set prefix from BOL till end of q.INS */
			prfx = line;
			prfz = q.ins + q.inz + !!q.inz - line ;
			/* check if we've got him in our books */
			if (nbook || zbook) {
				hx = hash(q.ins, q.inz);
				for (k = 0U; k < nbook; k++) {
					if (conx[k] == hx) {
						goto unwnd;
					}
				}
			}
			if (nctch) {
				goto unwnd;
			} else if (!zbook) {
				/* ok, it's not for us */
				continue;
			} else if (UNLIKELY(nbook >= zbook)) {
				/* resize */
				zbook *= 2U;
				conx = realloc(conx, zbook * sizeof(*conx));
				book = realloc(book, zbook * sizeof(*book));
			}
			/* initialise the book */
			conx[nbook] = hx, book[nbook] = make_xbook(), nbook++;
		unwnd:
			/* we have to unwind second levels manually
			 * because we need to print the interim steps */
			if (UNLIKELY(q.q.f == BOOK_LVL_1 &&
				     (prq == prq2 || prq == prq3))) {
				book_iter_t i = book_iter(book[k].book, q.q.s);
				while (book_iter_next(&i) &&
				       (q.q.s == BOOK_SIDE_BID && i.p > q.q.p ||
					q.q.s == BOOK_SIDE_ASK && i.p < q.q.p)) {
					book_quo_t r = {
						q.q.s, BOOK_LVL_2,
						.p = i.p,
						.q = 0.dd
					};
					o = book_add(book[k].book, r);
					prq(book + k, r, o);
				}
			} else if (UNLIKELY(q.q.s == BOOK_SIDE_CLR)) {
				if (UNLIKELY(prq == prq2 || prq == prq3)) {
					/* do it manually so we can print
					 * the interim steps */
					book_iter_t i;

					i = book_iter(book[k].book, BOOK_SIDE_BID);
					while (book_iter_next(&i)) {
						book_quo_t r = {
							BOOK_SIDE_BID,
							BOOK_LVL_2,
							.p = i.p,
							.q = 0.dd,
						};
						o = book_add(book[k].book, r);
						prq(book + k, r, o);
					}

					i = book_iter(book[k].book, BOOK_SIDE_ASK);
					while (book_iter_next(&i)) {
						book_quo_t r = {
							BOOK_SIDE_ASK,
							BOOK_LVL_2,
							.p = i.p,
							.q = 0.dd,
						};
						o = book_add(book[k].book, r);
						prq(book + k, r, o);
					}
					continue;
				}
			}
			/* add to book */
			o = book_add(book[k].book, q.q);
			/* printx */
			prq(book + k, q.q, o);
		}
		free(line);
	}

	if (nbook + nctch) {
		for (size_t i = 0U; i < nbook + nctch; i++) {
			book[i] = free_xbook(book[i]);
		}
		free(conx);
		free(book);
	}

out:
	yuck_free(argi);
	return rc;
}

/* book2book.c ends here */
