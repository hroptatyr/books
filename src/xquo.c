/*** xquo.c -- quotes serialising/deserialising
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
#include "xquo.h"
#include "nifty.h"

#define strtopx		strtod64
#define pxtostr		d64tostr
#define strtoqx		strtod64
#define qxtostr		d64tostr

#if defined __INTEL_COMPILER
# pragma warning (push)
# pragma warning (disable: 1419)
# pragma warning (disable: 2203)
#endif	/* __INTEL_COMPILER */

extern void *memrchr(void const *s, int c_in, size_t n);
#include "memrchr.c"

#if defined __INTEL_COMPILER
# pragma warning (pop)
#endif	/* __INTEL_COMPILER */


tv_t
strtotv(const char *ln, char **endptr)
{
	char *on;
	tv_t r;

	/* time value up first */
	with (long unsigned int s, x) {
		if (UNLIKELY(!(s = strtoul(ln, &on, 10)) || on == NULL)) {
			r = NATV;
			goto out;
		} else if (*on == '.') {
			char *moron;

			x = strtoul(++on, &moron, 10);
			if (UNLIKELY(moron - on > 9U)) {
				return NATV;
			} else if ((moron - on) % 3U) {
				/* huh? */
				return NATV;
			}
			switch (moron - on) {
			default:
			case 0U:
				x *= MSECS;
			case 3U:
				x *= MSECS;
			case 6U:
				x *= MSECS;
			case 9U:
				/* all is good */
				break;
			}
			on = moron;
		} else {
			x = 0U;
		}
		r = s * NSECS + x;
	}
out:
	if (LIKELY(endptr != NULL)) {
		*endptr = on;
	}
	return r;
}

ssize_t
tvtostr(char *restrict buf, size_t bsz, tv_t t)
{
	long unsigned int ts = t / NSECS;
	long unsigned int tn = t % NSECS;
	size_t i;

	if (UNLIKELY(bsz < 19U)) {
		return 0U;
	}

	buf[0U] = '0';
	for (i = !ts; ts > 0U; ts /= 10U, i++) {
		buf[i] = (ts % 10U) ^ '0';
	}
	/* revert buffer */
	for (char *bp = buf + i - 1, *ap = buf, tmp; bp > ap;
	     tmp = *bp, *bp-- = *ap, *ap++ = tmp);
	/* nanoseconds, fixed size */
	buf[i] = '.';
	for (size_t j = 9U; j > 0U; tn /= 10U, j--) {
		buf[i + j] = (tn % 10U) ^ '0';
	}
	return i + 10U;
}

xquo_t
read_xquo(const char *line, size_t llen)
{
/* process one line */
	char *lp, *on;
	xquo_t q;

	/* get timestamp */
	q.q.t = strtotv(line, NULL);

	/* get qty */
	if (UNLIKELY((lp = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without quantity */
		return NOT_A_XQUO;
	}
	llen = lp - line;
	q.q.q = strtoqx(lp + 1U, NULL);

	/* get prc */
	if (UNLIKELY((lp = memrchr(line, '\t', llen)) == NULL)) {
		/* can't do without price */
		return NOT_A_XQUO;
	}
	llen = lp - line;
	q.q.p = strtopx(lp + 1U, &on);
	if (UNLIKELY(on <= lp + 1U)) {
		/* invalidate price */
		q.q.p = NANPX;
	}

	/* get flavour, should be just before ON */
	with (unsigned char f = *(unsigned char*)--lp) {
		/* map 1, 2, 3 to LVL_{1,2,3}
		 * everything else goes to LVL_0 */
		f ^= '0';
		q.q.f = (typeof(q.q.f))(f & -(f < 4U));
	}

	/* rewind manually */
	for (; lp > line && lp[-1] != '\t'; lp--);
	with (unsigned char s = *(unsigned char*)lp) {
		/* map A or a to ASK and B or b to BID
		 * map C to CLR, D to DEL (and T for TRA to DEL)
		 * everything else goes to SIDE_UNK */
		s &= ~0x20U;
		s &= (unsigned char)-(s ^ '@' < NBOOK_SIDES || s == 'T');
		s &= 0xfU;
		q.q.s = (book_side_t)s;

		if (UNLIKELY(!q.q.s)) {
			/* cannot put entry to either side, just ignore */
			return NOT_A_XQUO;
		}
	}

	if (LIKELY(lp-- > line)) {
		llen = lp - line;
		if (LIKELY((q.ins = memrchr(line, '\t', llen)) != NULL)) {
			q.ins++;
		} else {
			q.ins = line;
		}
		q.inz = lp - q.ins;
	} else {
		q.ins = line, q.inz = 0U;
	}

	return q;
}

/* xquo.c ends here */
