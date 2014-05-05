/*-
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

/*
 * __wt_kv_return --
 *	Return a page referenced key/value pair to the application.
 */
int
__wt_kv_return(WT_SESSION_IMPL *session, WT_CURSOR_BTREE *cbt)
{
	WT_BTREE *btree;
	WT_CELL *cell;
	WT_CELL_UNPACK unpack;
	WT_CURSOR *cursor;
	WT_PAGE *page;
	WT_ROW *rip;
	WT_UPDATE *upd;
	uint8_t v;

	btree = S2BT(session);

	page = cbt->ref->page;
	cursor = &cbt->iface;

	switch (page->type) {
	case WT_PAGE_COL_FIX:
		/*
		 * The interface cursor's record has usually been set, but that
		 * isn't universally true, specifically, cursor.search_near may
		 * call here without first setting the interface cursor.
		 */
		cursor->recno = cbt->recno;

		/*
		 * If the cursor references a WT_INSERT item, take the related
		 * WT_UPDATE item.
		 */
		if (cbt->ins != NULL &&
		    (upd = __wt_txn_read(session, cbt->ins->upd)) != NULL) {
			cursor->value.data = WT_UPDATE_DATA(upd);
			cursor->value.size = upd->size;
			return (0);
		}
		v = __bit_getv_recno(page, cbt->iface.recno, btree->bitcnt);
		return (__wt_buf_set(session, &cursor->value, &v, 1));
	case WT_PAGE_COL_VAR:
		/*
		 * The interface cursor's record has usually been set, but that
		 * isn't universally true, specifically, cursor.search_near may
		 * call here without first setting the interface cursor.
		 */
		cursor->recno = cbt->recno;

		/*
		 * If the cursor references a WT_INSERT item, take the related
		 * WT_UPDATE item.
		 */
		if (cbt->ins != NULL &&
		    (upd = __wt_txn_read(session, cbt->ins->upd)) != NULL) {
			cursor->value.data = WT_UPDATE_DATA(upd);
			cursor->value.size = upd->size;
			return (0);
		}
		cell = WT_COL_PTR(page, &page->pg_var_d[cbt->slot]);
		break;
	case WT_PAGE_ROW_LEAF:
		rip = &page->pg_row_d[cbt->slot];

		/*
		 * If the cursor references a WT_INSERT item, take the key and
		 * related WT_UPDATE item.   Otherwise, take the key from the
		 * original page, and the value from any related WT_UPDATE item,
		 * or the page if the key was never updated.
		 */
		if (cbt->ins == NULL) {
			cursor->key.data = cbt->search_key.data;
			cursor->key.size = cbt->search_key.size;
			upd = __wt_txn_read(session, WT_ROW_UPDATE(page, rip));
		} else {
			cursor->key.data = WT_INSERT_KEY(cbt->ins);
			cursor->key.size = WT_INSERT_KEY_SIZE(cbt->ins);
			upd = __wt_txn_read(session, cbt->ins->upd);
		}
		if (upd != NULL) {
			cursor->value.data = WT_UPDATE_DATA(upd);
			cursor->value.size = upd->size;
			return (0);
		}

		/* Take the original cell (which may be empty). */
		if ((cell = __wt_row_leaf_value(page, rip)) == NULL) {
			cursor->value.size = 0;
			return (0);
		}
		break;
	WT_ILLEGAL_VALUE(session);
	}

	/* The value is an on-page cell, unpack and expand it as necessary. */
	__wt_cell_unpack(cell, &unpack);
	WT_RET(__wt_page_cell_data_ref(session, page, &unpack, &cursor->value));

	return (0);
}
