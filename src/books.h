/*** books.h -- order books and stuff
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
#if !defined INCLUDED_books_h_
#include <stdlib.h>
#include <stdbool.h>

#if !defined BOOKSD32 || !defined BOOKSD64
# define books_h_once
#endif

#if defined books_h_once
typedef long long unsigned int tv_t;
#define NATV	((tv_t)-1ULL)
#define NSECS	(1000000000)
#define USECS	(1000000)
#define MSECS	(1000)
#endif

#if !defined BOOKSD32 && !defined BOOKSD64
/* set a default */
# define BOOKSD64
#endif	/* BOOKSD64 */

#undef NANPX
#undef isnanpx
#undef NANQX
#undef isnanqx
#undef INFPX
#undef isinfpx

#undef book_quo_t
#undef book_iter_t
#undef make_book
#undef free_book
#undef book_add
#undef book_clr
#undef book_exp
#undef book_top
#undef book_tops
#undef book_ctop
#undef book_ctops
#undef book_vtop
#undef book_vtops
#undef book_pdo
#undef book_iter
#undef book_iter_next
#undef px_t

#if 0

#elif defined BOOKSD32
# define px_t		_Decimal32
typedef _Decimal64 qx_t;
#
# define NANPX		NAND32
# define isnanpx	isnand32
# define NANQX		NAND64
# define isnanqx	isnand64
# define INFPX		INFD32
# define isinfpx	isinfd32
#
# define book_quo_t	bookd32_quo_t
# define book_iter_t	bookd32_iter_t
# define make_book	make_bookd32
# define free_book	free_bookd32
# define book_add	bookd32_add
# define book_clr	bookd32_clr
# define book_exp	bookd32_exp
# define book_top	bookd32_top
# define book_tops	bookd32_tops
# define book_ctop	bookd32_ctop
# define book_ctops	bookd32_ctops
# define book_vtop	bookd32_vtop
# define book_vtops	bookd32_vtops
# define book_pdo	bookd32_pdo
# define book_iter	bookd32_iter
# define book_iter_next	bookd32_iter_next

#elif defined BOOKSD64
# define px_t		_Decimal64
typedef _Decimal64 qx_t;
#
# define NANPX		NAND64
# define isnanpx	isnand64
# define NANQX		NAND64
# define isnanqx	isnand64
# define INFPX		INFD64
# define isinfpx	isinfd64
#
# define book_quo_t	bookd64_quo_t
# define book_iter_t	bookd64_iter_t
# define make_book	make_bookd64
# define free_book	free_bookd64
# define book_add	bookd64_add
# define book_clr	bookd64_clr
# define book_exp	bookd64_exp
# define book_top	bookd64_top
# define book_tops	bookd64_tops
# define book_ctop	bookd64_ctop
# define book_ctops	bookd64_ctops
# define book_vtop	bookd64_vtop
# define book_vtops	bookd64_vtops
# define book_pdo	bookd64_pdo
# define book_iter	bookd64_iter
# define book_iter_next	bookd64_iter_next
#endif	/* BOOKSD32 || BOOKSD64 */

/* our books look like
 * T... INS ACT PRC QTY
 * with T being timestamps, the earliest first
 * INS being an instrument identifier
 * ACT being
 *   - B1/A1 for top-level bid/ask,
 *   - B2/A2 for book bid/ask
 *   - B3/A3 for changes to the book (bid/ask)
 *   - c1 for combined top-level (order is bid ask bsz asz)
 *   - cN for combined price level N
 *   - CQ for consolidated book at quantity Q
 *   - m1 for mid-points within the top-level
 *   - MQ for mid-points within consolidation of quantity Q
 *   - T0 for trades (at ask) and S0 for sells at bid */

#if defined books_h_once
typedef enum {
	BOOK_SIDE_UNK,
	BOOK_SIDE_ASK,
	BOOK_SIDE_BID,
	BOOK_SIDE_CLR,
	BOOK_SIDE_DEL,
	NBOOK_SIDES
} book_side_t;

typedef enum {
	BOOK_LVL_0,
	BOOK_LVL_1,
	BOOK_LVL_2,
	BOOK_LVL_3,
} book_lvl_t;

typedef struct {
	void *quos[2U];
} book_t;

typedef struct {
	qx_t base;
	qx_t term;
	tv_t yngt;
	tv_t oldt;
} book_pdo_t;
#endif

typedef struct {
	book_side_t s;
	book_lvl_t f;
	px_t p;
	qx_t q;
	tv_t t;
} book_quo_t;

#define NOT_A_QUO	(book_quo_t){BOOK_SIDE_UNK}
#define NOT_A_QUO_P(x)	!((x).s)

typedef struct {
	void *b;
	size_t i;
	px_t p;
	qx_t q;
	tv_t t;
} book_iter_t;

#define BIDX(x)		((x) - 1U)
#define BOOK(x)		quos[BIDX(x)]


extern book_t make_book(void);
extern book_t free_book(book_t);

/**
 * Add QUO to BOOK.
 * QUO will hold the previous state, i.e. the old price level
 * in p, the old quantity in q and the old time in t, respectively. */
extern book_quo_t book_add(book_t, book_quo_t);

/**
 * Clear the entire book. */
extern void book_clr(book_t);

/**
 * Expunge all quotes older than T. */
extern void book_exp(book_t, tv_t);

/**
 * Return the top-most quote of BOOK'S SIDE. */
extern book_quo_t book_top(book_t, book_side_t);

/**
 * Put the top-most N price levels into PX (and QX) and return the number
 * of levels filled. */
extern size_t
book_tops(px_t*restrict, qx_t*restrict, book_t, book_side_t, size_t n);

/**
 * Return the consolidated quote of BOOK'S SIDE that equals or exceeds Q. */
extern book_quo_t book_ctop(book_t, book_side_t, qx_t Q);

/**
 * Put the top-most N consolidated price levels into PX (and QX) and return
 * the number of levels filled.  Level i equals or exceeds i*Q in quantity. */
extern size_t
book_ctops(px_t*restrict, qx_t*restrict, book_t, book_side_t, qx_t Q, size_t n);

/**
 * Return the value-consolidated quote of BOOK'S SIDE that equals or exceeds V.
 * Value is calculated as price times quantity. */
extern book_quo_t book_vtop(book_t, book_side_t, qx_t V);

/**
 * Put the top-most N value-consolidated price levels into PX (and QX) and
 * return the number of levels filled.
 * Level i equals or exceeds i*V in value. */
extern size_t
book_vtops(px_t*restrict, qx_t*restrict, book_t, book_side_t, qx_t V, size_t n);

extern bool book_iter_next(book_iter_t*);

/**
 * Like book_ctop() but produce a base/term aggregate and limit book
 * traversal to LMT. */
extern book_pdo_t book_pdo(book_t, book_side_t, qx_t q, px_t lmt);


static inline book_iter_t
book_iter(book_t b, book_side_t s)
{
	return (book_iter_t){b.BOOK(s)};
}

#define INCLUDED_books_h_
#undef books_h_once
#endif	/* INCLUDED_books_h_ */
