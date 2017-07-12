/*** books.c -- order books and stuff
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
#if defined HAVE_DFP754_H
# include <dfp754.h>
#endif	/* HAVE_DFP754_H */
#include "dfp754_d64.h"
#include "books.h"
#include "btree.h"
#include "nifty.h"

#define quantizepx	quantized64
#define quantizeqx	quantized64


book_t
make_book(void)
{
	book_t r = {
		.quos = {
			[BIDX(SIDE_ASK)] = make_btree(false),
			[BIDX(SIDE_BID)] = make_btree(true),
		}
	};
	return r;
}

book_t
free_book(book_t b)
{
	free_btree(b.quos[0U]);
	free_btree(b.quos[1U]);
	return (book_t){};
}

quo_t
book_add(book_t b, quo_t q)
{
	if (UNLIKELY(q.s == SIDE_CLR)) {
		book_clr(b);
		goto out;
	}
	switch (q.f) {
		qx_t tmp;
	case LVL_3:
		tmp = btree_add(b.BOOK(q.s), q.p, q.q);
		q.o = tmp - q.q;
		q.q = tmp;
		break;
	case LVL_2:
		q.o = btree_put(b.BOOK(q.s), q.p, q.q);
		break;
	case LVL_1:
		if (UNLIKELY(q.q < 0.dd)) {
			/* what an odd level-1 quote */
			return NOT_A_QUO;
		} else if (LIKELY(!isnanpx(q.p))) {
			/* we'd have to pop anything more top-level
			 * in the books
			 * we put the value first so it's guaranteed
			 * to be in there */
			q.o = btree_put(b.BOOK(q.s), q.p, q.q);
		}
		/* now iter away anything from top that isn't our quote */
		for (btree_iter_t i = {.t = b.BOOK(q.s)};
		     btree_iter_next(&i) && i.k != q.p;) {
			(void)btree_put(b.BOOK(q.s), i.k, 0.dd);
		}
		break;
	case LVL_0:
	default:
		/* we don't know what to do */
		return NOT_A_QUO;
	}
out:
	return q;
}

void
book_clr(book_t b)
{
	btree_clr(b.BOOK(SIDE_BID));
	btree_clr(b.BOOK(SIDE_ASK));
	return;
}

quo_t
book_top(book_t b, side_t s)
{
	btree_iter_t i = {.t = b.BOOK(s)};

	if (UNLIKELY(!btree_iter_next(&i))) {
		return NOT_A_QUO;
	}
	return (quo_t){.s = s, .f = LVL_1, .p = i.k, .q = i.v};
}

size_t
book_tops(px_t *restrict p, qx_t *restrict q, book_t b, side_t s, size_t n)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	size_t j;

	if (UNLIKELY(q == NULL)) {
		goto only_p;
	}

	for (j = 0U; j < n && btree_iter_next(&i); j++) {
		p[j] = i.k;
		q[j] = i.v;
	}
	return j;

only_p:
	for (j = 0U; j < n && btree_iter_next(&i); j++) {
		p[j] = i.k;
	}
	return j;
}

quo_t
book_ctop(book_t b, side_t s, qx_t q)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t P = 0.dd, Q = 0.dd;

	for (; Q < q && btree_iter_next(&i);) {
		P += i.k * i.v;
		Q += i.v;
	}
	if (UNLIKELY(Q < q)) {
		return NOT_A_QUO;
	} else if (LIKELY(Q > q)) {
		P -= i.k * (Q - q);
	}
	return (quo_t){.s = s, .f = LVL_1,
			.p = quantizepx((px_t)(P / q), i.k),
			.q = quantizeqx(q, i.v)
			};
}

size_t
book_ctops(px_t *restrict p, qx_t *restrict q,
	   book_t b, side_t s, qx_t Q, size_t n)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t c = 0.dd, C = 0.dd;
	size_t j;
	qx_t R;

	if (UNLIKELY(q == NULL)) {
		goto only_p;
	}

	for (j = 0U, R = Q; j < n; j++, R += Q) {
		for (; C < R && btree_iter_next(&i);) {
			c += i.k * i.v;
			C += i.v;
		}
		if (UNLIKELY(C < R)) {
			break;
		}
		p[j] = quantizepx((px_t)((c - i.k * (C - R)) / R), i.k);
		q[j] = quantizeqx(R, i.v);
	}
	return j;

only_p:
	for (j = 0U, R = Q; j < n; j++, R += Q) {
		for (; C < R && btree_iter_next(&i);) {
			c += i.k * i.v;
			C += i.v;
		}
		if (UNLIKELY(C < R)) {
			break;
		}
		p[j] = quantizepx((px_t)((c - i.k * (C - R)) / R), i.k);
	}
	return j;
}

quo_t
book_vtop(book_t b, side_t s, qx_t v)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t P = 0.dd, Q = 0.dd;

	for (; P < v && btree_iter_next(&i);) {
		P += i.k * i.v;
		Q += i.v;
	}
	if (UNLIKELY(P < v)) {
		return NOT_A_QUO;
	} else if (LIKELY(P > v)) {
		/* P - v is exactly what we open too much */
		Q -= (P - v) / i.k;
	}
	return (quo_t){.s = s, .f = LVL_1,
			.p = quantizepx((px_t)(v / Q), i.k),
			.q = quantizeqx(Q, i.v)
			};
}

size_t
book_vtops(px_t *restrict p, qx_t *restrict q,
	   book_t b, side_t s, qx_t v, size_t n)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t c = 0.dd, C = 0.dd;
	size_t j;
	qx_t r;

	if (UNLIKELY(q == NULL)) {
		goto only_p;
	}

	for (j = 0U, r = v; j < n; j++, r += v) {
		qx_t Q;
		for (; c < r && btree_iter_next(&i);) {
			c += i.k * i.v;
			C += i.v;
		}
		if (UNLIKELY(c < r)) {
			break;
		}
		Q = C - (c - r) / i.k;
		p[j] = quantizepx((px_t)(r / Q), i.k);
		q[j] = quantizeqx(Q, i.v);
	}
	return j;

only_p:
	for (j = 0U, r = v; j < n; j++, r += v) {
		qx_t Q;
		for (; c < r && btree_iter_next(&i);) {
			c += i.k * i.v;
			C += i.v;
		}
		if (UNLIKELY(c < r)) {
			break;
		}
		Q = C - (c - r) / i.k;
		p[j] = quantizepx((px_t)(r / Q), i.k);
	}
	return j;
}


bool
book_iter_next(book_iter_t *iter)
{
	return btree_iter_next((void*)iter);
}

/* books.c ends here */
