#include <stdio.h>
#include "books.h"
#include "nifty.h"


int
main(void)
{
	book_t b;

	b = make_book();

	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 200.0dd, 300.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 198.0dd, 100.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 197.0dd, 300.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 196.0dd, 100.dd});

	book_quo_t f1 = book_top(b, BOOK_SIDE_ASK);
	book_quo_t f2 = book_top(b, BOOK_SIDE_BID);

	printf("%f %f  %f %f  %f %f  %f %f\n",
	       (double)f1.p, (double)f1.q,
	       (double)f2.p, (double)f2.q);

	free_book(b);
	return f1.p != 198.0dd || f1.q != 100.dd ||
		f2.p != 197.0dd || f2.q != 300.dd;
}
