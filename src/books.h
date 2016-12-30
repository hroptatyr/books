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
#define INCLUDED_books_h_

#if !defined KEY_T
# define KEY_T	_Decimal32
#endif	/* !KEY_T */
#if !defined VAL_T
# define VAL_T	_Decimal64
#endif	/* !VAL_T */

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

#endif	/* INCLUDED_books_h_ */
