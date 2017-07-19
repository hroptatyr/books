/*** btree_val.h -- values and keys of btrees ***/
#if !defined INCLUDED_btree_val_h_
#define INCLUDED_btree_val_h_
#include <stdbool.h>

/* keys are prices */
typedef _Decimal64 btree_key_t;

/* values are plqu's and a plqu_val_t for the sum */
typedef struct {
	_Decimal64 q;
	long long unsigned int t;
} btree_val_t;

#define btree_val_nil	((btree_val_t){0.dd})

static inline bool
btree_val_nil_p(btree_val_t v)
{
	return v.q <= 0.dd;
}

static inline void
free_btree_val(btree_val_t v)
{
	(void)v;
	return;
}

#endif	/* INCLUDED_btree_val_h_ */
