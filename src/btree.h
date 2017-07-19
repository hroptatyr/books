/*** btree.h -- simple b+tree impl
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
#if !defined INCLUDED_btree_h_
#define INCLUDED_btree_h_
#include <stdlib.h>
#include <stdbool.h>
#include "books.h"

#if !defined KEY_T
# define KEY_T	_Decimal64
#endif	/* !KEY_T */

#if !defined VAL_T
typedef struct {
	_Decimal64 q;
	long long unsigned int t;
} btree_val_t;

static inline bool
btree_val_nil_p(btree_val_t v)
{
	return v.q <= 0.dd;
}

# define VAL_0	((btree_val_t){0.dd})
# define VAL_T	btree_val_t
#endif	/* !VAL_T */

typedef struct btree_s *btree_t;

typedef struct {
	btree_t t;
	size_t i;
	KEY_T k;
	VAL_T v;
} btree_iter_t;


extern btree_t make_btree(bool descp);
extern void free_btree(btree_t);

extern VAL_T btree_add(btree_t, KEY_T, VAL_T);
extern VAL_T btree_put(btree_t, KEY_T, VAL_T);
extern void btree_clr(btree_t);

extern KEY_T btree_min(btree_t, VAL_T*);
extern KEY_T btree_max(btree_t, VAL_T*);
extern KEY_T btree_top(btree_t, VAL_T*);
extern KEY_T btree_bot(btree_t, VAL_T*);

extern bool btree_iter_next(btree_iter_t*);

#endif	/* INCLUDED_btree_h_ */
