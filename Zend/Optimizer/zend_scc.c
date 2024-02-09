/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Dmitry Stogov <dmitry@php.net>                              |
   +----------------------------------------------------------------------+
*/

#include "zend_compile.h"
#include "zend_worklist.h"
#include "zend_optimizer_internal.h"

#define CHECK_SCC_VAR(var2) \
	do { \
		if (!ssa->vars[var2].no_val) { \
			if (ssa->vars[var2].scc < 0) { \
				zend_ssa_check_scc_var(op_array, ssa, var2, index, stack); \
			} \
			if (ssa->vars[var2].scc < ssa->vars[var].scc) { \
				ssa->vars[var].scc = ssa->vars[var2].scc; \
				is_root = 0; \
			} \
		} \
	} while (0)

#define CHECK_SCC_ENTRY(var2) \
	do { \
		if (ssa->vars[var2].scc != ssa->vars[var].scc) { \
			ssa->vars[var2].scc_entry = 1; \
		} \
	} while (0)

#define ADD_SCC_VAR(_var) \
	do { \
		if (ssa->vars[_var].scc == scc && \
		    !(ssa->var_info[_var].type & MAY_BE_REF)) { \
			zend_bitset_incl(worklist, _var); \
		} \
	} while (0)

#define ADD_SCC_VAR_1(_var) \
	do { \
		if (ssa->vars[_var].scc == scc && \
		    !(ssa->var_info[_var].type & MAY_BE_REF) && \
		    !zend_bitset_in(visited, _var)) { \
			zend_bitset_incl(worklist, _var); \
		} \
	} while (0)

#if 0
/* Recursive Pearce's SCC algorithm implementation */
static void zend_ssa_check_scc_var(const zend_op_array *op_array, zend_ssa *ssa, int var, int *index, zend_worklist_stack *stack) /* {{{ */
{
	int is_root = 1;
#ifdef SYM_RANGE
	zend_ssa_phi *p;
#endif

	ssa->vars[var].scc = *index;
	(*index)++;

	FOR_EACH_VAR_USAGE(var, CHECK_SCC_VAR);

#ifdef SYM_RANGE
	/* Process symbolic control-flow constraints */
	p = ssa->vars[var].sym_use_chain;
	while (p) {
		CHECK_SCC_VAR(p->ssa_var);
		p = p->sym_use_chain;
	}
#endif

	if (is_root) {
		ssa->sccs--;
		while (stack->len > 0) {
			int var2 = zend_worklist_stack_peek(stack);
			if (ssa->vars[var2].scc < ssa->vars[var].scc) {
				break;
			}
			zend_worklist_stack_pop(stack);
			ssa->vars[var2].scc = ssa->sccs;
			(*index)--;
		}
		ssa->vars[var].scc = ssa->sccs;
		ssa->vars[var].scc_entry = 1;
		(*index)--;
	} else {
		zend_worklist_stack_push(stack, var);
	}
}
/* }}} */

ZEND_API void zend_ssa_find_sccs(const zend_op_array *op_array, zend_ssa *ssa) /* {{{ */
{
	int index = 0;
	zend_worklist_stack stack;
	int j;
	ALLOCA_FLAG(stack_use_heap)

	ZEND_WORKLIST_STACK_ALLOCA(&stack, ssa->vars_count, stack_use_heap);

	/* Find SCCs using Pearce's algorithm. */
	ssa->sccs = ssa->vars_count;
	for (j = 0; j < ssa->vars_count; j++) {
		if (!ssa->vars[j].no_val && ssa->vars[j].scc < 0) {
			zend_ssa_check_scc_var(op_array, ssa, j, &index, &stack);
		}
	}

	if (ssa->sccs) {
		/* Shift SCC indexes. */
		for (j = 0; j < ssa->vars_count; j++) {
			if (ssa->vars[j].scc >= 0) {
				ssa->vars[j].scc -= ssa->sccs;
			}
		}
	}
	ssa->sccs = ssa->vars_count - ssa->sccs;

	for (j = 0; j < ssa->vars_count; j++) {
		if (ssa->vars[j].scc >= 0) {
			int var = j;
			FOR_EACH_VAR_USAGE(var, CHECK_SCC_ENTRY);
		}
	}

	ZEND_WORKLIST_STACK_FREE_ALLOCA(&stack, stack_use_heap);
}
/* }}} */

#else
/* Iterative Pearce's SCC algorithm implementation */

typedef struct _zend_scc_iterator {
	int               state;
	int               last;
	union {
		int           use;
		zend_ssa_phi *phi;
	};
} zend_scc_iterator;

static int zend_scc_next(const zend_op_array *op_array, zend_ssa *ssa, int var, zend_scc_iterator *iterator) /* {{{ */
{
	zend_ssa_phi *phi;
	int use, var2;

	switch (iterator->state) {
		case 0:                       goto state_0;
		case 1:  use = iterator->use; goto state_1;
		case 2:  use = iterator->use; goto state_2;
		case 3:  use = iterator->use; goto state_3;
		case 4:  use = iterator->use; goto state_4;
		case 5:  use = iterator->use; goto state_5;
		case 6:  use = iterator->use; goto state_6;
		case 7:  use = iterator->use; goto state_7;
		case 8:  use = iterator->use; goto state_8;
		case 9:  phi = iterator->phi; goto state_9;
#ifdef SYM_RANGE
		case 10: phi = iterator->phi; goto state_10;
#endif
		case 11:                      goto state_11;
	}

state_0:
	use = ssa->vars[var].use_chain;
	while (use >= 0) {
		iterator->use = use;
		var2 = ssa->ops[use].op1_def;
		if (var2 >= 0 && !ssa->vars[var2].no_val) {
			iterator->state = 1;
			return var2;
		}
state_1:
		var2 = ssa->ops[use].op2_def;
		if (var2 >= 0 && !ssa->vars[var2].no_val) {
			iterator->state = 2;
			return var2;
		}
state_2:
		var2 = ssa->ops[use].result_def;
		if (var2 >= 0 && !ssa->vars[var2].no_val) {
			iterator->state = 3;
			return var2;
		}
state_3:
		if (op_array->opcodes[use].opcode == ZEND_OP_DATA) {
			var2 = ssa->ops[use-1].op1_def;
			if (var2 >= 0 && !ssa->vars[var2].no_val) {
				iterator->state = 4;
				return var2;
			}
state_4:
			var2 = ssa->ops[use-1].op2_def;
			if (var2 >= 0 && !ssa->vars[var2].no_val) {
				iterator->state = 5;
				return var2;
			}
state_5:
			var2 = ssa->ops[use-1].result_def;
			if (var2 >= 0 && !ssa->vars[var2].no_val) {
				iterator->state = 8;
				return var2;
			}
		} else if ((uint32_t)use+1 < op_array->last &&
		           op_array->opcodes[use+1].opcode == ZEND_OP_DATA) {
			var2 = ssa->ops[use+1].op1_def;
			if (var2 >= 0 && !ssa->vars[var2].no_val) {
				iterator->state = 6;
				return var2;
			}
state_6:
			var2 = ssa->ops[use+1].op2_def;
			if (var2 >= 0 && !ssa->vars[var2].no_val) {
				iterator->state = 7;
				return var2;
			}
state_7:
			var2 = ssa->ops[use+1].result_def;
			if (var2 >= 0 && !ssa->vars[var2].no_val) {
				iterator->state = 8;
				return var2;
			}
		}
state_8:
		use = zend_ssa_next_use(ssa->ops, var, use);
	}

	phi = ssa->vars[var].phi_use_chain;
	while (phi) {
		var2 = phi->ssa_var;
		if (!ssa->vars[var2].no_val) {
			iterator->state = 9;
			iterator->phi = phi;
			return var2;
		}
state_9:
		phi = zend_ssa_next_use_phi(ssa, var, phi);
	}

#ifdef SYM_RANGE
	/* Process symbolic control-flow constraints */
	phi = ssa->vars[var].sym_use_chain;
	while (phi) {
		var2 = phi->ssa_var;
		if (!ssa->vars[var2].no_val) {
			iterator->state = 10;
			iterator->phi = phi;
			return var2;
		}
state_10:
		phi = phi->sym_use_chain;
	}
#endif

	iterator->state = 11;
state_11:
	return -1;
}
/* }}} */

static void zend_ssa_check_scc_var(const zend_op_array *op_array, zend_ssa *ssa, int var, int *index, zend_worklist_stack *stack, zend_worklist_stack *vstack, zend_scc_iterator *iterators) /* {{{ */
{
restart:
	zend_worklist_stack_push(vstack, var);
	iterators[var].state = 0;
	iterators[var].last = -1;
	ssa->vars[var].scc_entry = 1;
	ssa->vars[var].scc = *index;
	(*index)++;

	while (vstack->len > 0) {
		var = zend_worklist_stack_peek(vstack);
		while (1) {
			int var2;

			if (iterators[var].last >= 0) {
				/* finish edge */
				var2 = iterators[var].last;
				if (ssa->vars[var2].scc < ssa->vars[var].scc) {
					ssa->vars[var].scc = ssa->vars[var2].scc;
					ssa->vars[var].scc_entry = 0;
				}
			}
			var2 = zend_scc_next(op_array, ssa, var, iterators + var);
			iterators[var].last = var2;
			if (var2 < 0) break;
			/* begin edge */
			if (ssa->vars[var2].scc < 0) {
				var = var2;
				goto restart;
			}
		}

		/* finish visiting */
		zend_worklist_stack_pop(vstack);
		if (ssa->vars[var].scc_entry) {
			ssa->sccs--;
			while (stack->len > 0) {
				int var2 = zend_worklist_stack_peek(stack);
				if (ssa->vars[var2].scc < ssa->vars[var].scc) {
					break;
				}
				zend_worklist_stack_pop(stack);
				ssa->vars[var2].scc = ssa->sccs;
				(*index)--;
			}
			ssa->vars[var].scc = ssa->sccs;
			(*index)--;
		} else {
			zend_worklist_stack_push(stack, var);
		}
	}
}
/* }}} */

ZEND_API void zend_ssa_find_sccs(const zend_op_array *op_array, zend_ssa *ssa) /* {{{ */
{
	int index = 0;
	zend_worklist_stack stack, vstack;
	zend_scc_iterator *iterators;
	int j;
	ALLOCA_FLAG(stack_use_heap)
	ALLOCA_FLAG(vstack_use_heap)
	ALLOCA_FLAG(iterators_use_heap)

	iterators = do_alloca(sizeof(zend_scc_iterator) * ssa->vars_count, iterators_use_heap);
	ZEND_WORKLIST_STACK_ALLOCA(&vstack, ssa->vars_count, vstack_use_heap);
	ZEND_WORKLIST_STACK_ALLOCA(&stack, ssa->vars_count, stack_use_heap);

	/* Find SCCs using Pearce's algorithm. */
	ssa->sccs = ssa->vars_count;
	for (j = 0; j < ssa->vars_count; j++) {
		if (!ssa->vars[j].no_val && ssa->vars[j].scc < 0) {
			zend_ssa_check_scc_var(op_array, ssa, j, &index, &stack, &vstack, iterators);
		}
	}

	if (ssa->sccs) {
		/* Shift SCC indexes. */
		for (j = 0; j < ssa->vars_count; j++) {
			if (ssa->vars[j].scc >= 0) {
				ssa->vars[j].scc -= ssa->sccs;
			}
		}
	}
	ssa->sccs = ssa->vars_count - ssa->sccs;

	for (j = 0; j < ssa->vars_count; j++) {
		if (ssa->vars[j].scc >= 0) {
			int var = j;
			FOR_EACH_VAR_USAGE(var, CHECK_SCC_ENTRY);
		}
	}

	ZEND_WORKLIST_STACK_FREE_ALLOCA(&stack, stack_use_heap);
	ZEND_WORKLIST_STACK_FREE_ALLOCA(&vstack, vstack_use_heap);
	free_alloca(iterators, iterators_use_heap);
}
/* }}} */

#endif


