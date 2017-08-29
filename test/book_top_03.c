#include <stdio.h>
#include "books.h"
#include "nifty.h"


int
main(void)
{
	book_t b;
	px_t bp[4U], ap[4U];
	qx_t bq[4U], aq[4U];

	b = make_book();

	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 200.0dd, 300.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 198.0dd, 100.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 197.0dd, 300.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 196.0dd, 100.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 197.0dd, 100.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 198.0dd, 100.dd});

	size_t a2 = book_tops(ap, aq, b, BOOK_SIDE_ASK, 2);
	size_t b4 = book_tops(bp, bq, b, BOOK_SIDE_BID, 4);

	free_book(b);
	return a2 != 2U || ap[0U] != 197.0dd || aq[0U] != 100.dd ||
		ap[1U] != 198.0dd || aq[1U] != 100.dd ||
		b4 != 3U || bp[0U] != 198.0dd || bq[0U] != 100.dd ||
		bp[1U] != 197.0dd || bq[1U] != 300.dd ||
		bp[2U] != 196.0dd || bq[2U] != 100.dd;
}
