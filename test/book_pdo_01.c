#include <stdio.h>
#include "books.h"
#include "dfp754_d64.h"
#include "nifty.h"

#define NANPX	NAND64


int
main(void)
{
	book_t b;

	b = make_book();

	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 200.0dd, 300.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_ASK, BOOK_LVL_2, 198.0dd, 100.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 197.0dd, 300.dd});
	book_add(b, (book_quo_t){BOOK_SIDE_BID, BOOK_LVL_2, 196.0dd, 100.dd});

	book_pdo_t f1 = book_pdo(b, BOOK_SIDE_ASK, 200.dd, NANPX);
	book_pdo_t f2 = book_pdo(b, BOOK_SIDE_ASK, 500.dd, 205.dd);
	book_pdo_t f3 = book_pdo(b, BOOK_SIDE_BID, 100.dd, NANPX);
	book_pdo_t f4 = book_pdo(b, BOOK_SIDE_BID, 400.dd, 197.dd);

	printf("%f %f  %f %f  %f %f  %f %f\n",
	       (double)f1.base, (double)f1.term,
	       (double)f2.base, (double)f2.term,
	       (double)f3.base, (double)f3.term,
	       (double)f4.base, (double)f4.term);

	free_book(b);
	return f1.base != 200.dd || f1.term != 39800.dd ||
		f2.base != 400.dd || f2.term != 79800.dd ||
		f3.base != 100.dd || f3.term != 19700.dd ||
		f4.base != 300.dd || f4.term != 59100.dd;
}
