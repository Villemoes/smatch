/*
 * smatch/check_propagate.c
 *
 * Copyright (C) 2010 Dan Carpenter.
 *
 * Licensed under the Open Software License version 1.1
 *
 */

/*
 * People should just return the error codes they recieved 
 * instead of making up there own dumb error codes all the time.
 */

#include "smatch.h"

static int my_id;

static struct expression *last_return;
static struct expression *last_func;

static char *get_fn_name(struct expression *expr)
{
	if (expr->type != EXPR_CALL)
		return NULL;
	if (expr->fn->type != EXPR_SYMBOL)
		return NULL;
	return get_variable_from_expr(expr->fn, NULL);
}

static void match_call_assignment(struct expression *expr)
{
	if (get_macro_name(&expr->left->pos))
		return;
	last_return = expr->left;
	last_func = expr->right;
}

static void match_unset(struct expression *expr)
{
	last_return = NULL;
}

static void match_return(struct expression *ret_value)
{
	long long rval;
	long long lret;
	char *name;

	if (!get_value(ret_value, &rval) || rval >= 0)
		return;
	if (get_implied_value(last_return, &lret))
		return;
	if (!get_implied_max(last_return, &lret) || lret >= 0)
		return;
	if (get_implied_min(last_return, &lret))
		return;
	name = get_variable_from_expr(last_return, NULL);
	sm_msg("info: why not propagate '%s' from %s() instead of %lld?",
		name, get_fn_name(last_func), rval);
	free_string(name);
}

void check_propagate(int id)
{
	if (option_project != PROJ_KERNEL)
		return;

	my_id = id;
	add_hook(&match_unset, ASSIGNMENT_HOOK);
	add_hook(&match_call_assignment, CALL_ASSIGNMENT_HOOK);
	add_hook(&match_return, RETURN_HOOK);
}