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
#include "dfp754_d32.h"
#include "dfp754_d64.h"
#if !defined BOOKSD64 && !defined BOOKSD32
# define BOOKS_MULTI
# define BOOKSD64
#endif	/* !BOOKSD64 && !BOOKSD32 */
#include "books.h"
#include "btree.h"
#include "nifty.h"

#if !defined BOOKSD32 || !defined BOOKSD64
# define books_c_once
#endif

#if 0

#elif defined BOOKSD32
#define quantizepx	quantized32
#elif defined BOOKSD64
#define quantizepx	quantized64
#endif
#define quantizeqx	quantized64


#if defined books_c_once
static inline __attribute__((pure, const)) tv_t
min_tv(tv_t t1, tv_t t2)
{
	return t1 <= t2 ? t1 : t2;
}

static inline __attribute__((pure, const)) tv_t
max_tv(tv_t t1, tv_t t2)
{
	return t1 >= t2 ? t1 : t2;
}
#endif


book_t
make_book(void)
{
	book_t r = {
		.quos = {
			[BIDX(BOOK_SIDE_ASK)] = make_btree(false),
			[BIDX(BOOK_SIDE_BID)] = make_btree(true),
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

book_quo_t
book_add(book_t b, book_quo_t q)
{
	switch (q.s) {
	case BOOK_SIDE_BID:
	case BOOK_SIDE_ASK:
		/* proceed with level treatment */
		switch (q.f) {
			btree_val_t *tmp;
			qx_t o;
			tv_t t;
		case BOOK_LVL_3:
			tmp = btree_put(b.BOOK(q.s), q.p);
			o = tmp->q;
			t = tmp->t;
			q.q += o;
			tmp->q = q.q >= 0.df ? q.q : 0.df;
			tmp->t = q.t;
			q.q = o;
			q.t = t;
			break;
		case BOOK_LVL_2:
			tmp = btree_put(b.BOOK(q.s), q.p);
			o = tmp->q;
			t = tmp->t;
			tmp->q = q.q;
			tmp->t = q.t;
			q.q = o;
			q.t = t;
			break;
		case BOOK_LVL_1:
			if (UNLIKELY(q.q < 0.df)) {
				/* what an odd level-1 quote */
				return NOT_A_QUO;
			}
			btree_clr(b.BOOK(q.s));
			if (UNLIKELY(isnanpx(q.p))) {
				break;
			}
			/* we'd have to pop anything more top-level
			 * in the books ...
			 * we put the value first so it's guaranteed
			 * to be in there */
			tmp = btree_put(b.BOOK(q.s), q.p);
			o = tmp->q;
			t = tmp->t;
			tmp->q = q.q;
			tmp->t = q.t;
			q.q = o;
			q.t = t;
			break;
		case BOOK_LVL_0:
		default:
			goto inv;
		}
		break;
	case BOOK_SIDE_CLR:
		book_clr(b);
		break;
	case BOOK_SIDE_DEL:
		for (btree_iter_t i = {.t = b.BOOK(BOOK_SIDE_ASK)};
		     btree_iter_next(&i) && i.k <= q.p;) {
			i.v->q = i.k < q.p ? 0.df : i.v->q - q.q;
		}
		for (btree_iter_t i = {.t = b.BOOK(BOOK_SIDE_BID)};
		     btree_iter_next(&i) && i.k >= q.p;) {
			i.v->q = i.k > q.p ? 0.df : i.v->q - q.q;
		}
		break;
	default:
		goto inv;
	}
	return q;

inv:
	/* we don't know what to do */
	return NOT_A_QUO;
}

void
book_clr(book_t b)
{
	btree_clr(b.BOOK(BOOK_SIDE_BID));
	btree_clr(b.BOOK(BOOK_SIDE_ASK));
	return;
}

void
book_exp(book_t b, tv_t t)
{
	if (UNLIKELY(t == 0ULL)) {
		return;
	} else if (UNLIKELY(t == NATV)) {
		book_clr(b);
		return;
	}
	/* otherwise */
	for (btree_iter_t i = {b.BOOK(BOOK_SIDE_ASK)}; btree_iter_next(&i);) {
		if (i.v->t <= t) {
			*i.v = btree_val_nil;
		}
	}
	for (btree_iter_t i = {b.BOOK(BOOK_SIDE_BID)}; btree_iter_next(&i);) {
		if (i.v->t <= t) {
			*i.v = btree_val_nil;
		}
	}
	return;
}

book_quo_t
book_top(book_t b, book_side_t s)
{
	btree_iter_t i = {.t = b.BOOK(s)};

	if (UNLIKELY(!btree_iter_next(&i))) {
		return NOT_A_QUO;
	}
	return (book_quo_t){
		.s = s, .f = BOOK_LVL_1, .p = i.k, .q = i.v->q, .t = i.v->t
	};
}

size_t
book_tops(px_t *restrict p, qx_t *restrict q, book_t b, book_side_t s, size_t n)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	size_t j;

	if (UNLIKELY(q == NULL)) {
		goto only_p;
	}

	for (j = 0U; j < n && btree_iter_next(&i); j++) {
		p[j] = i.k;
		q[j] = i.v->q;
	}
	return j;

only_p:
	for (j = 0U; j < n && btree_iter_next(&i); j++) {
		p[j] = i.k;
	}
	return j;
}

book_quo_t
book_ctop(book_t b, book_side_t s, qx_t q)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t P = 0.df, Q = 0.df;

	for (; Q < q && btree_iter_next(&i);) {
		P += i.k * i.v->q;
		Q += i.v->q;
	}
	if (UNLIKELY(Q < q)) {
		return NOT_A_QUO;
	} else if (LIKELY(Q > q)) {
		P -= i.k * (Q - q);
	}
	return (book_quo_t){
		.s = s, .f = BOOK_LVL_1,
		.p = quantizepx((px_t)(P / q), i.k),
		.q = quantizeqx(q, i.v->q),
		.t = i.v->t,
	};
}

size_t
book_ctops(px_t *restrict p, qx_t *restrict q,
	   book_t b, book_side_t s, qx_t Q, size_t n)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t c = 0.df, C = 0.df;
	size_t j;
	qx_t R;

	if (UNLIKELY(q == NULL)) {
		goto only_p;
	}

	for (j = 0U, R = Q; j < n; j++, R += Q) {
		for (; C < R && btree_iter_next(&i);) {
			c += i.k * i.v->q;
			C += i.v->q;
		}
		if (UNLIKELY(C < R)) {
			break;
		}
		p[j] = quantizepx((px_t)((c - i.k * (C - R)) / R), i.k);
		q[j] = quantizeqx(R, i.v->q);
	}
	return j;

only_p:
	for (j = 0U, R = Q; j < n; j++, R += Q) {
		for (; C < R && btree_iter_next(&i);) {
			c += i.k * i.v->q;
			C += i.v->q;
		}
		if (UNLIKELY(C < R)) {
			break;
		}
		p[j] = quantizepx((px_t)((c - i.k * (C - R)) / R), i.k);
	}
	return j;
}

book_quo_t
book_vtop(book_t b, book_side_t s, qx_t v)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t P = 0.df, Q = 0.df;

	for (; P < v && btree_iter_next(&i);) {
		P += i.k * i.v->q;
		Q += i.v->q;
	}
	if (UNLIKELY(P < v)) {
		return NOT_A_QUO;
	} else if (LIKELY(P > v)) {
		/* P - v is exactly what we open too much */
		Q -= (P - v) / i.k;
	}
	return (book_quo_t){
		.s = s, .f = BOOK_LVL_1,
		.p = quantizepx((px_t)(v / Q), i.k),
		.q = quantizeqx(Q, i.v->q),
		.t = i.v->t,
	};
}

size_t
book_vtops(px_t *restrict p, qx_t *restrict q,
	   book_t b, book_side_t s, qx_t v, size_t n)
{
	btree_iter_t i = {.t = b.BOOK(s)};
	qx_t c = 0.df, C = 0.df;
	size_t j;
	qx_t r;

	if (UNLIKELY(q == NULL)) {
		goto only_p;
	}

	for (j = 0U, r = v; j < n; j++, r += v) {
		qx_t Q;
		for (; c < r && btree_iter_next(&i);) {
			c += i.k * i.v->q;
			C += i.v->q;
		}
		if (UNLIKELY(c < r)) {
			break;
		}
		Q = C - (c - r) / i.k;
		p[j] = quantizepx((px_t)(r / Q), i.k);
		q[j] = quantizeqx(Q, i.v->q);
	}
	return j;

only_p:
	for (j = 0U, r = v; j < n; j++, r += v) {
		qx_t Q;
		for (; c < r && btree_iter_next(&i);) {
			c += i.k * i.v->q;
			C += i.v->q;
		}
		if (UNLIKELY(c < r)) {
			break;
		}
		Q = C - (c - r) / i.k;
		p[j] = quantizepx((px_t)(r / Q), i.k);
	}
	return j;
}


book_pdo_t
book_pdo(book_t b, book_side_t s, qx_t q, px_t lmt)
{
	book_pdo_t r = {.base = 0.df, .term = 0.df, .yngt = 0U, .oldt = NATV};

	/* nan to +/- inf */
	lmt = !isnanpx(lmt) ? lmt
		: s == BOOK_SIDE_BID ? -INFPX
		: s == BOOK_SIDE_ASK ? INFPX
		: lmt;
	for (btree_iter_t i = {.t = b.BOOK(s)};
	     q > 0.df && btree_iter_next(&i) &&
		     (s == BOOK_SIDE_BID && i.k >= lmt ||
		      s == BOOK_SIDE_ASK && i.k <= lmt);) {
		qx_t Q = i.v->q <= q ? i.v->q : q;
		r.term += i.k * Q;
		r.base += Q;
		r.yngt = max_tv(r.yngt, i.v->t);
		r.oldt = min_tv(r.oldt, i.v->t);
		q -= Q;
	}
	return r;
}


bool
book_iter_next(book_iter_t *iter)
{
	btree_iter_t *i = (void*)iter;
	if (btree_iter_next(i)) {
		btree_val_t *tmp = i->v;
		iter->q = tmp->q;
		iter->t = tmp->t;
		return true;
	}
	return false;
}

#undef books_c_once
#if defined BOOKS_MULTI
# if defined BOOKSD64 && !defined BOOKSD32
#  define BOOKSD32
#
#  undef quantizepx
#  undef INCLUDED_books_h_
#  undef INCLUDED_btree_h_
#  include __FILE__
# endif
#endif

/* books.c ends here */
