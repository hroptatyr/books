/*** booksnap.c -- book snapper
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
#include <assert.h>
#include "hash.h"
#include "books.h"
#include "nifty.h"

#define NSECS	(1000000000)
#define MSECS	(1000)

typedef long unsigned int tv_t;
#define NOT_A_TIME	((tv_t)-1ULL)

#define strtopx		strtod32
#define pxtostr		d32tostr
#define strtoqx		strtod64
#define qxtostr		d64tostr

typedef struct {
	tv_t t;
	quo_t q;
	const char *ins;
	size_t inz;
} xquo_t;

#define NOT_A_XQUO	((xquo_t){NOT_A_TIME, NOT_A_QUO})
#define NOT_A_XQUO_P(x)	(NOT_A_QUO_P((x).q))

#define HX_CATCHALL	((hx_t)-1ULL)

/* command line params */
static tv_t intv = 1U * MSECS;
static tv_t offs = 0U * MSECS;
static FILE *sfil;


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

static tv_t
strtotv(const char *ln, char **endptr)
{
	char *on;
	tv_t r;

	/* time value up first */
	with (long unsigned int s, x) {
		if (UNLIKELY(!(s = strtoul(ln, &on, 10)) || on == NULL)) {
			r = NOT_A_TIME;
			goto out;
		} else if (*on == '.') {
			char *moron;

			x = strtoul(++on, &moron, 10);
			if (UNLIKELY(moron - on > 9U)) {
				return NOT_A_TIME;
			} else if ((moron - on) % 3U) {
				/* huh? */
				return NOT_A_TIME;
			}
			switch (moron - on) {
			case 9U:
				x /= MSECS;
			case 6U:
				x /= MSECS;
			case 3U:
				break;
			case 0U:
			default:
				break;
			}
			on = moron;
		} else {
			x = 0U;
		}
		r = s * MSECS + x;
	}
out:
	if (LIKELY(endptr != NULL)) {
		*endptr = on;
	}
	return r;
}

static ssize_t
tvtostr(char *restrict buf, size_t bsz, tv_t t)
{
	return snprintf(buf, bsz, "%lu.%03lu000000", t / MSECS, t % MSECS);
}

static inline size_t
memncpy(char *restrict tgt, const char *src, size_t zrc)
{
	memcpy(tgt, src, zrc);
	return zrc;
}


static tv_t metr;
static tv_t(*next)(tv_t);

static tv_t
_next_intv(tv_t newm)
{
	return newm;
}

static tv_t
_next_stmp(tv_t newm)
{
	static char *line;
	static size_t llen;

	if (getline(&line, &llen, sfil) > 0 &&
	    (newm = strtotv(line, NULL)) != NOT_A_TIME) {
		return newm - 1ULL;
	}
	/* otherwise it's the end of the road */
	free(line);
	line = NULL;
	llen = 0UL;
	return NOT_A_TIME;
}

static xquo_t
rdq(const char *line, size_t llen)
{
/* process one line */
	char *on;
	xquo_t q;

	/* get timestamp */
	if (UNLIKELY((q.t = strtotv(line, NULL)) == NOT_A_TIME)) {
		return NOT_A_XQUO;
	}

	/* get qty */
	if (UNLIKELY((on = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without quantity */
		return NOT_A_XQUO;
	}
	llen = on - line;
	q.q.q = strtoqx(on + 1U, NULL);

	/* get prc */
	if (UNLIKELY((on = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without price */
		return NOT_A_XQUO;
	}
	llen = on - line;
	q.q.p = strtopx(on + 1U, NULL);

	/* get flavour, should be just before ON */
	with (unsigned char f = *(unsigned char*)--on) {
		/* map 1, 2, 3 to LVL_{1,2,3}
		 * everything else goes to LVL_0 */
		f ^= '0';
		q.q.f = (typeof(q.q.f))(f & -(f < 4U));
	}

	/* rewind manually */
	for (; on > line && on[-1] != '\t'; on--);
	with (unsigned char s = *(unsigned char*)on) {
		/* map A or a to ASK and B or b to BID
		 * everything else goes to SIDE_UNK */
		s &= ~0x20U;
		s ^= '@';
		q.q.s = (side_t)(s & -(s <= 2U));

		if (UNLIKELY(!q.q.s)) {
			/* cannot put entry to either side, just ignore */
			return NOT_A_XQUO;
		}
	}
	llen = on - line;

	/* see if we've got pairs */
	q.ins = memrchr(line, '\t', llen - 1U) ?: deconst(line - 1U);
	q.ins++;
	q.inz = on - 1U - q.ins;
	return q;
}

static void
snap(book_t bk, const char *cont)
{
	char buf[256U];
	size_t len, prfz;

	if (UNLIKELY(!metr)) {
		return;
	}

	len = tvtostr(buf, sizeof(buf), (metr + 1ULL) * intv + offs);
	if (LIKELY(cont != NULL)) {
		buf[len++] = '\t';
		len += memncpy(buf + len, cont, strlen(cont));
	}
	buf[len++] = '\t';
	buf[len++] = 'B';
	buf[len++] = '2';
	buf[len++] = '\t';
	prfz = len;

	for (book_iter_t i = {.b = bk.BOOK(SIDE_BID)};
	     book_iter_next(&i); len = prfz) {
		len += pxtostr(buf + len, sizeof(buf) - len, i.p);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, i.q);
		buf[len++] = '\n';
		/* and out */
		fwrite(buf, 1, len, stdout);
	}

	/* go to asks */
	buf[prfz - 3U] = 'A';
	for (book_iter_t i = {.b = bk.BOOK(SIDE_ASK)};
	     book_iter_next(&i); len = prfz) {
		len += pxtostr(buf + len, sizeof(buf) - len, i.p);
		buf[len++] = '\t';
		len += qxtostr(buf + len, sizeof(buf) - len, i.q);
		buf[len++] = '\n';
		/* and out */
		fwrite(buf, 1, len, stdout);
	}
	return;
}


#include "booksnap.yucc"

int
main(int argc, char *argv[])
{
	static yuck_t argi[1U];
	int rc = EXIT_SUCCESS;
	static hx_t *conx;
	static const char **cont;
	static book_t *book;
	static size_t nbook;
	static size_t zbook;
	size_t nctch = 0U;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = EXIT_FAILURE;
		goto out;
	}

	if (argi->interval_arg) {
		char *on;

		if (!(intv = strtoul(argi->interval_arg, &on, 10))) {
			errno = 0, serror("\
Error: cannot read interval argument, must be positive.");
			rc = EXIT_FAILURE;
			goto out;
		}
		switch (*on) {
		case '\0':
		case 's':
		case 'S':
			/* user wants seconds, do they not? */
			intv *= MSECS;
			break;
		case 'm':
		case 'M':
			switch (*++on) {
			case '\0':
				/* they want minutes, oh oh */
				intv *= 60UL * MSECS;
				break;
			case 's':
			case 'S':
				/* milliseconds it is then */
				intv = intv;
				break;
			default:
				goto invalid_intv;
			}
			break;
		case 'h':
		case 'H':
			/* them hours we use */
			intv *= 60UL * 60UL * MSECS;
			break;
		default:
		invalid_intv:
			errno = 0, serror("\
Error: invalid suffix in interval, use `ms', `s', `m', or `h'");
			rc = EXIT_FAILURE;
			goto out;
		}
	}

	if (argi->offset_arg) {
		char *on;
		long int o;

		o = strtol(argi->offset_arg, &on, 10);

		switch (*on) {
		case '\0':
		case 's':
		case 'S':
			/* user wants seconds, do they not? */
			o *= MSECS;
			break;
		case 'm':
		case 'M':
			switch (*++on) {
			case '\0':
				/* they want minutes, oh oh */
				o *= 60U * MSECS;
				break;
			case 's':
			case 'S':
				/* milliseconds it is then */
				o = o;
				break;
			default:
				goto invalid_offs;
			}
			break;
		case 'h':
		case 'H':
			/* them hours we use */
			o *= 60U * 60U * MSECS;
			break;
		default:
		invalid_offs:
			errno = 0, serror("\
Error: invalid suffix in offset, use `ms', `s', `m', or `h'");
			rc = EXIT_FAILURE;
			goto out;
		}

		/* canonicalise */
		if (argi->stamps_arg) {
			/* offset has a special meaning in stamps mode */
			offs = o;
		} else if (o > 0) {
			offs = o % intv;
		} else if (o < 0) {
			offs = intv - (-o % intv);
		}
	}

	if (argi->stamps_arg) {
		if (UNLIKELY((sfil = fopen(argi->stamps_arg, "r")) == NULL)) {
			serror("\
Error: cannot open stamps file");
			rc = EXIT_FAILURE;
			goto out;
		}
		/* reset intv to unit interval */
		intv = 1ULL;
	}

	/* use a next routine du jour */
	next = !argi->stamps_arg ? _next_intv : _next_stmp;

	if ((nbook = argi->instr_nargs)) {
		size_t j = 0U;

		/* initialise hash array and books */
		cont = malloc(nbook * sizeof(*cont));
		conx = malloc(nbook * sizeof(*conx));
		book = malloc(nbook * sizeof(*book));

		for (size_t i = 0U; i < nbook; i++) {
			const char *this = argi->instr_args[i];
			const size_t conz = strlen(this);

			if (UNLIKELY(conz == 0U ||
				     conz == 1U && *this == '*')) {
				/* catch-all hash */
				continue;
			}
			cont[j] = this;
			conx[j] = hash(this, conz);
			book[j] = make_book();
			j++;
		}
		if (j < nbook) {
			/* oh, there's been catch-alls,
			 * shrink NBOOK and initialise last cell */
			nbook = j;
			cont[nbook] = nbook ? "ALL" : NULL;
			conx[nbook] = HX_CATCHALL;
			book[nbook] = make_book();
			nctch = 1U;
		}
	} else {
		/* allocate some 8U books */
		zbook = 8U;
		cont = malloc(zbook * sizeof(*cont));
		conx = malloc(zbook * sizeof(*conx));
		book = malloc(zbook * sizeof(*book));
	}

	{
		char *line = NULL;
		size_t llen = 0UL;

		for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
			xquo_t q;
			size_t k;
			hx_t hx;

			if (NOT_A_XQUO_P(q = rdq(line, nrd))) {
				/* invalid quote line */
				continue;
			}
			/* check if we've got him in our books */
			if (nbook || zbook) {
				hx = hash(q.ins, q.inz);
				for (k = 0U; k < nbook; k++) {
					if (conx[k] == hx) {
						goto snap;
					}
				}
			}
			if (nctch) {
				goto snap;
			} else if (!zbook) {
				/* ok, it's not for us */
				continue;
			} else if (UNLIKELY(nbook >= zbook)) {
				/* resize */
				zbook *= 2U;
				cont = realloc(cont, zbook * sizeof(*cont));
				conx = realloc(conx, zbook * sizeof(*conx));
				book = realloc(book, zbook * sizeof(*book));
			}
			/* initialise the book */
			cont[nbook] = strndup(q.ins, q.inz),
				conx[nbook] = hx,
				book[nbook] = make_book(),
				nbook++;
		snap:
			/* align metronome to interval */
			q.t--;
			q.t -= offs;
			q.t /= intv;

			/* do we need to shoot a snap? */
			for (; UNLIKELY(q.t > metr); metr = next(q.t)) {
				/* materialise snapshot */
				for (size_t i = 0U; i < nbook + nctch; i++) {
					snap(book[i], cont[i]);
				}
			}

			/* add to book */
			q.q = book_add(book[k], q.q);
		}
		free(line);
		/* final snapshot */
		for (size_t i = 0U; i < nbook + nctch; i++) {
			snap(book[i], cont[i]);
		}
	}

	if (nbook + nctch) {
		for (size_t i = 0U; i < nbook + nctch; i++) {
			book[i] = free_book(book[i]);
		}
		if (zbook) {
			for (size_t i = 0U; i < nbook + nctch; i++) {
				free(deconst(cont[i]));
			}
		}
		free(cont);
		free(conx);
		free(book);
	}

	if (argi->stamps_arg) {
		fclose(sfil);
	}

out:
	yuck_free(argi);
	return rc;
}

/* booksnap.c ends here */