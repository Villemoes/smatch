/*
 * sparse/smatch_extra_helper.c
 *
 * Copyright (C) 2009 Dan Carpenter.
 *
 * Licensed under the Open Software License version 1.1
 *
 */

#include "parse.h"
#include "smatch.h"
#include "smatch_extra.h"
#include "smatch_slist.h"

ALLOCATOR(data_info, "smatch extra data");
ALLOCATOR(data_range, "data range");

static char *show_num(long long num)
{
	static char buff[256];

	if (num == whole_range.min) {
		snprintf(buff, 255, "min");
	} else if (num == whole_range.max) {
		snprintf(buff, 255, "max");
	} else if (num < 0) {
		snprintf(buff, 255, "(%lld)", num);
	} else {
		snprintf(buff, 255, "%lld", num);
	}
	buff[255] = '\0';
	return buff;
}

char *show_ranges(struct range_list *list)
{
	struct data_range *tmp;
	char full[256];
	int i = 0;

	full[0] = '\0';
	full[255] = '\0';
 	FOR_EACH_PTR(list, tmp) {
		if (i++)
			strncat(full, ",", 254 - strlen(full));
		if (tmp->min == tmp->max) {
			strncat(full, show_num(tmp->min), 254 - strlen(full));
			continue;
		}
		strncat(full, show_num(tmp->min), 254 - strlen(full));
		strncat(full, "-", 254 - strlen(full));
		strncat(full, show_num(tmp->max), 254 - strlen(full));
	} END_FOR_EACH_PTR(tmp);
	return alloc_sname(full);
}

static struct data_range range_zero = {
	.min = 0,
	.max = 0,
};

static struct data_range range_one = {
	.min = 1,
	.max = 1,
};

struct data_range *alloc_range(long long min, long long max)
{
	struct data_range *ret;

	if (min > max) {
		printf("Error invalid range %lld to %lld\n", min, max);
	}
	if (min == whole_range.min && max == whole_range.max)
		return &whole_range;
	if (min == 0 && max == 0)
		return &range_zero;
	if (min == 1 && max == 1)
		return &range_one;
	ret = __alloc_data_range(0);
	ret->min = min;
	ret->max = max;
	return ret;
}

struct data_info *alloc_dinfo_range(long long min, long long max)
{
	struct data_info *ret;

	ret = __alloc_data_info(0);
	ret->type = DATA_RANGE;
	ret->merged = 0;
	ret->value_ranges = NULL;
	add_range(&ret->value_ranges, min, max);
	return ret;
}

void add_range(struct range_list **list, long long min, long long max)
{
	struct data_range *tmp = NULL;
	struct data_range *new;
	
 	FOR_EACH_PTR(*list, tmp) {
		if (max != whole_range.max && max + 1 == tmp->min) {
			/* join 2 ranges into a big range */
			new = alloc_range(min, tmp->max);
			REPLACE_CURRENT_PTR(tmp, new);
			return;
		}
		if (max < tmp->min) {  /* new range entirely below */
			new = alloc_range(min, max);
			INSERT_CURRENT(new, tmp);
			return;
		}
		if (min < tmp->min) { /* new range partially below */
			if (max < tmp->max)
				max = tmp->max;
			new = alloc_range(min, max);
			REPLACE_CURRENT_PTR(tmp, new);
			return;
		}
		if (max <= tmp->max) /* new range already included */
			return;
		if (min <= tmp->max) { /* new range partially above */
			min = tmp->min;
			new = alloc_range(min, max);
			REPLACE_CURRENT_PTR(tmp, new);
			return;
		}
		if (min != whole_range.min && min - 1 == tmp->max) {
			/* join 2 ranges into a big range */
			new = alloc_range(tmp->min, max);
			REPLACE_CURRENT_PTR(tmp, new);
			return;
		}
	} END_FOR_EACH_PTR(tmp);
	new = alloc_range(min, max);
	add_ptr_list(list, new);
}

struct range_list *clone_range_list(struct range_list *list)
{
	struct data_range *tmp;
	struct range_list *ret = NULL;

	FOR_EACH_PTR(list, tmp) {
		add_ptr_list(&ret, tmp);
	} END_FOR_EACH_PTR(tmp);
	return ret;
}

struct range_list *range_list_union(struct range_list *one, struct range_list *two)
{
	struct data_range *tmp;
	struct range_list *ret = NULL;

	if (!one || !two)  /*having nothing in a list means everything is in */
		return NULL;

	FOR_EACH_PTR(one, tmp) {
		add_range(&ret, tmp->min, tmp->max);
	} END_FOR_EACH_PTR(tmp);
	FOR_EACH_PTR(two, tmp) {
		add_range(&ret, tmp->min, tmp->max);
	} END_FOR_EACH_PTR(tmp);
	return ret;
}

struct range_list *remove_range(struct range_list *list, long long min, long long max)
{
	struct data_range *tmp;
	struct range_list *ret = NULL;

	FOR_EACH_PTR(list, tmp) {
		if (tmp->max < min) {
			add_range(&ret, tmp->min, tmp->max);
			continue;
		}
		if (tmp->min > max) {
			add_range(&ret, tmp->min, tmp->max);
			continue;
		}
		if (tmp->min >= min && tmp->max <= max)
			continue;
		if (tmp->min >= min) {
			add_range(&ret, max + 1, tmp->max);
		} else if (tmp->max <= max) {
			add_range(&ret, tmp->min, min - 1);
		} else {
			add_range(&ret, tmp->min, min - 1);
			add_range(&ret, max + 1, tmp->max);
		}
	} END_FOR_EACH_PTR(tmp);
	return ret;
}

long long get_dinfo_min(struct data_info *dinfo)
{
	struct data_range *drange;

	if (!dinfo || !dinfo->value_ranges)
		return whole_range.min;
	drange = first_ptr_list((struct ptr_list *)dinfo->value_ranges);
	return drange->min;
}

long long get_dinfo_max(struct data_info *dinfo)
{
	struct data_range *drange;

	if (!dinfo || !dinfo->value_ranges)
		return whole_range.max;
	drange = first_ptr_list((struct ptr_list *)dinfo->value_ranges);
	return drange->max;
}

int is_whole_range(struct range_list *ranges)
{
	struct data_range *drange;

	if (!ranges)
		return 0;

	drange = first_ptr_list((struct ptr_list *)ranges);
	if (drange->min == whole_range.min && drange->max == whole_range.max)
		return 1;
	return 0;
}

/* 
 * if it can be only one value return that, else return UNDEFINED
 */
long long get_single_value_from_range(struct data_info *dinfo)
{
	struct data_range *tmp;
	int count = 0;
	long long ret = UNDEFINED;

	if (dinfo->type != DATA_RANGE)
		return UNDEFINED;

	FOR_EACH_PTR(dinfo->value_ranges, tmp) {
		if (!count++) {
			if (tmp->min != tmp->max)
				return UNDEFINED;
			ret = tmp->min;
		} else {
			return UNDEFINED;
		}
	} END_FOR_EACH_PTR(tmp);
	return ret;
}

static int true_comparison_range(struct data_range *left, int comparison, struct data_range *right)
{
	switch(comparison){
	case '<':
	case SPECIAL_UNSIGNED_LT:
		if (left->min < right->max)
			return 1;
		return 0;
	case SPECIAL_UNSIGNED_LTE:
	case SPECIAL_LTE:
		if (left->min <= right->max)
			return 1;
		return 0;
	case SPECIAL_EQUAL:
		if (left->max < right->min)
			return 0;
		if (left->min > right->max)
			return 0;
		return 1;
	case SPECIAL_UNSIGNED_GTE:
	case SPECIAL_GTE:
		if (left->max >= right->min)
			return 1;
		return 0;
	case '>':
	case SPECIAL_UNSIGNED_GT:
		if (left->max > right->min)
			return 1;
		return 0;
	case SPECIAL_NOTEQUAL:
		if (left->min != left->max)
			return 1;
		if (right->min != right->max)
			return 1;
		if (left->min != right->min)
			return 1;
		return 0;
	default:
		smatch_msg("unhandled comparison %d\n", comparison);
		return UNDEFINED;
	}
	return 0;
}

static int false_comparison_range(struct data_range *left, int comparison, struct data_range *right)
{
	switch(comparison){
	case '<':
	case SPECIAL_UNSIGNED_LT:
		if (left->max >= right->min)
			return 1;
		return 0;
	case SPECIAL_UNSIGNED_LTE:
	case SPECIAL_LTE:
		if (left->max > right->min)
			return 1;
		return 0;
	case SPECIAL_EQUAL:
		if (left->min != left->max)
			return 1;
		if (right->min != right->max)
			return 1;
		if (left->min != right->min)
			return 1;
		return 0;
	case SPECIAL_UNSIGNED_GTE:
	case SPECIAL_GTE:
		if (left->min < right->max)
			return 1;
		return 0;
	case '>':
	case SPECIAL_UNSIGNED_GT:
		if (left->min <= right->max)
			return 1;
		return 0;
	case SPECIAL_NOTEQUAL:
		if (left->max < right->min)
			return 0;
		if (left->min > right->max)
			return 0;
		return 1;
	default:
		smatch_msg("unhandled comparison %d\n", comparison);
		return UNDEFINED;
	}
	return 0;
}

int possibly_true(int comparison, struct data_info *dinfo, int num, int left)
{
	struct data_range *tmp;
	int ret = 0;
	struct data_range drange;

	drange.min = num;
	drange.max = num;

	FOR_EACH_PTR(dinfo->value_ranges, tmp) {
		if (left)
			ret = true_comparison_range(tmp, comparison, &drange);
		else
			ret = true_comparison_range(&drange,  comparison, tmp);
		if (ret)
			return ret;
	} END_FOR_EACH_PTR(tmp);
	return ret;
}

int possibly_false(int comparison, struct data_info *dinfo, int num, int left)
{
	struct data_range *tmp;
	int ret = 0;
	struct data_range drange;

	drange.min = num;
	drange.max = num;

	FOR_EACH_PTR(dinfo->value_ranges, tmp) {
		if (left)
			ret = false_comparison_range(tmp, comparison, &drange);
		else
			ret = false_comparison_range(&drange,  comparison, tmp);
		if (ret)
			return ret;
	} END_FOR_EACH_PTR(tmp);
	return ret;
}

static void free_single_dinfo(struct data_info *dinfo)
{
	if (dinfo->type == DATA_RANGE)
		__free_ptr_list((struct ptr_list **)&dinfo->value_ranges);
}

static void free_dinfos(struct allocation_blob *blob)
{
	unsigned int size = sizeof(struct data_info);
	unsigned int offset = 0;

	while (offset < blob->offset) {
		free_single_dinfo((struct data_info *)(blob->data + offset));
		offset += size;
	}
}

void free_data_info_allocs(void)
{
	struct allocator_struct *desc = &data_info_allocator;
	struct allocation_blob *blob = desc->blobs;

	desc->blobs = NULL;
	desc->allocations = 0;
	desc->total_bytes = 0;
	desc->useful_bytes = 0;
	desc->freelist = NULL;
	while (blob) {
		struct allocation_blob *next = blob->next;
		free_dinfos(blob);
		blob_free(blob, desc->chunking);
		blob = next;
	}
	clear_data_range_alloc();
}
