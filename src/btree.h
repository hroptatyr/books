/*** btree.h -- simple b+tree impl
 *
 * Copyright (C) 2016-2018 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of clob.
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
#include <stdlib.h>
#include <stdbool.h>
/* defines btree_val_t, hopefully */
#include "btree_val.h"

#if !defined BOOKSD32 && !defined BOOKSD64
/* a sane default */
# define BOOKSD64
#endif	/* !BOOKSD32 && !BOOKSD64 */

#undef btree_key_t
#undef btree_s
#undef btree_t
#undef btree_iter_t
#undef make_btree
#undef free_btree
#undef btree_get
#undef btree_put
#undef btree_rem
#undef btree_clr
#undef btree_top
#undef btree_iter_next

/* keys are prices */
#if 0

#elif defined BOOKSD32
# define btree_key_t	_Decimal32
# define btree_s	btreed32_s
# define btree_t	btreed32_t
# define btree_iter_t	btreed32_iter_t
# define make_btree	make_btreed32
# define free_btree	free_btreed32
# define btree_get	btreed32_get
# define btree_put	btreed32_put
# define btree_rem	btreed32_rem
# define btree_clr	btreed32_clr
# define btree_top	btreed32_top
# define btree_iter_next	btreed32_iter_next
#elif defined BOOKSD64
# define btree_key_t	_Decimal64
# define btree_s	btreed64_s
# define btree_t	btreed64_t
# define btree_iter_t	btreed64_iter_t
# define make_btree	make_btreed64
# define free_btree	free_btreed64
# define btree_get	btreed64_get
# define btree_put	btreed64_put
# define btree_rem	btreed64_rem
# define btree_clr	btreed64_clr
# define btree_top	btreed64_top
# define btree_iter_next	btreed64_iter_next
#endif	/* BOOKSD32 || BOOKSD64 */

typedef struct btree_s *btree_t;

typedef struct {
	btree_t t;
	size_t i;
	btree_key_t k;
	btree_val_t *v;
} btree_iter_t;


extern btree_t make_btree(bool descp);
extern void free_btree(btree_t);

extern btree_val_t *btree_get(btree_t, btree_key_t);
extern btree_val_t *btree_put(btree_t, btree_key_t);
extern btree_val_t btree_rem(btree_t, btree_key_t);
extern void btree_clr(btree_t);
extern btree_val_t *btree_top(btree_t, btree_key_t*);

extern bool btree_iter_next(btree_iter_t*);

#define INCLUDED_btree_h_
#endif	/* INCLUDED_btree_h_ */
