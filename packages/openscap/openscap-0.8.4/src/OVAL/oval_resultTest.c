/**
 * @file oval_resultTest.c
 * \brief Open Vulnerability and Assessment Language
 *
 * See more details at http://oval.mitre.org/
 */

/*
 * Copyright 2009-2010 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "David Niemoller" <David.Niemoller@g2-inc.com>
 *      "Peter Vrabec" <pvrabec@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <arpa/inet.h>
#if defined USE_REGEX_PCRE
#include <pcre.h>
#elif defined USE_REGEX_POSIX
#include <regex.h>
#endif
#include <ctype.h>
#include "oval_results_impl.h"
#include "oval_collection_impl.h"
#include "oval_string_map_impl.h"
#include "public/oval_types.h"
#include "common/util.h"
#include "common/debug_priv.h"
#include "common/_error.h"
#if !defined(__FreeBSD__)
# include <alloca.h>
#endif

static int rpmvercmp(const char *a, const char *b);	// don't really feel like creating a new header file just for this

/*
 * code from http://rpm.org/api/4.4.2.2/rpmvercmp_8c-source.html
 */

/* compare alpha and numeric segments of two versions */
/* return 1: a is newer than b */
/*        0: a and b are the same version */
/*       -1: b is newer than a */
static int rpmvercmp(const char *a, const char *b)
{
	char oldch1, oldch2;
	char *str1, *str2;
	char *one, *two;
	int rc;
	int isnum;

	/* easy comparison to see if versions are identical */
	if (!strcmp(a, b))
		return 0;

	/* TODO: make new oscap_alloca */
	str1 = alloca(strlen(a) + 1);
	str2 = alloca(strlen(b) + 1);

	strcpy(str1, a);
	strcpy(str2, b);

	one = str1;
	two = str2;

	/* loop through each version segment of str1 and str2 and compare them */
	while (*one && *two) {
		while (*one && !isalnum(*one))
			one++;
		while (*two && !isalnum(*two))
			two++;

		/* If we ran to the end of either, we are finished with the loop */
		if (!(*one && *two))
			break;

		str1 = one;
		str2 = two;

		/* grab first completely alpha or completely numeric segment */
		/* leave one and two pointing to the start of the alpha or numeric */
		/* segment and walk str1 and str2 to end of segment */
		if (isdigit(*str1)) {
			while (*str1 && isdigit(*str1))
				str1++;
			while (*str2 && isdigit(*str2))
				str2++;
			isnum = 1;
		} else {
			while (*str1 && isalpha(*str1))
				str1++;
			while (*str2 && isalpha(*str2))
				str2++;
			isnum = 0;
		}

		/* save character at the end of the alpha or numeric segment */
		/* so that they can be restored after the comparison */
		oldch1 = *str1;
		*str1 = '\0';
		oldch2 = *str2;
		*str2 = '\0';

		/* this cannot happen, as we previously tested to make sure that */
		/* the first string has a non-null segment */
		if (one == str1)
			return -1;	/* arbitrary */

		/* take care of the case where the two version segments are */
		/* different types: one numeric, the other alpha (i.e. empty) */
		/* numeric segments are always newer than alpha segments */
		/* result_test See patch #60884 (and details) from bugzilla #50977. */
		if (two == str2)
			return (isnum ? 1 : -1);

		if (isnum) {
			/* this used to be done by converting the digit segments */
			/* to ints using atoi() - it's changed because long  */
			/* digit segments can overflow an int - this should fix that. */

			/* throw away any leading zeros - it's a number, right? */
			while (*one == '0')
				one++;
			while (*two == '0')
				two++;

			/* whichever number has more digits wins */
			if (strlen(one) > strlen(two))
				return 1;
			if (strlen(two) > strlen(one))
				return -1;
		}

		/* strcmp will return which one is greater - even if the two */
		/* segments are alpha or if they are numeric.  don't return  */
		/* if they are equal because there might be more segments to */
		/* compare */
		rc = strcmp(one, two);
		if (rc)
			return (rc < 1 ? -1 : 1);

		/* restore character that was replaced by null above */
		*str1 = oldch1;
		one = str1;
		*str2 = oldch2;
		two = str2;
	}
	/* this catches the case where all numeric and alpha segments have */
	/* compared identically but the segment sepparating characters were */
	/* different */
	if ((!*one) && (!*two))
		return 0;

	/* whichever version still has characters left over wins */
	if (!*one)
		return -1;
	else
		return 1;
}

typedef struct oval_result_test {
	struct oval_result_system *system;
	struct oval_test *test;
	oval_result_t result;
	struct oval_collection *messages;
	struct oval_collection *items;
	struct oval_collection *bindings;
	int instance;
	bool bindings_initialized;
	bool bindings_clearable;
} oval_result_test_t;

struct oval_result_test *oval_result_test_new(struct oval_result_system *sys, char *tstid)
{
	oval_result_test_t *test = (oval_result_test_t *)
	    oscap_alloc(sizeof(oval_result_test_t));
	if (test == NULL)
		return NULL;

	struct oval_syschar_model *syschar_model = oval_result_system_get_syschar_model(sys);
	struct oval_definition_model *definition_model = oval_syschar_model_get_definition_model(syschar_model);
	test->system = sys;
	test->test = oval_definition_model_get_new_test(definition_model, tstid);
	test->messages = oval_collection_new();
	test->result = OVAL_RESULT_NOT_EVALUATED;
	test->instance = 1;
	test->items = oval_collection_new();
	test->bindings = oval_collection_new();
	test->bindings_initialized = false;
	test->bindings_clearable = false;
	return test;
}

struct oval_result_test *oval_result_test_clone
    (struct oval_result_system *new_system, struct oval_result_test *old_test) {
	__attribute__nonnull__(old_test);

	struct oval_test *oval_test = oval_result_test_get_test(old_test);
	char *testid = oval_test_get_id(oval_test);
	struct oval_result_test *new_test = oval_result_test_new(new_system, testid);
	struct oval_result_item_iterator *old_items = oval_result_test_get_items(old_test);
	while (oval_result_item_iterator_has_more(old_items)) {
		struct oval_result_item *old_item = oval_result_item_iterator_next(old_items);
		struct oval_result_item *new_item = oval_result_item_clone(new_system, old_item);
		oval_result_test_add_item(new_test, new_item);
	}
	oval_result_item_iterator_free(old_items);

	struct oval_variable_binding_iterator *old_bindings = oval_result_test_get_bindings(old_test);
	while (oval_variable_binding_iterator_has_more(old_bindings)) {
		struct oval_variable_binding *old_binding = oval_variable_binding_iterator_next(old_bindings);
		struct oval_variable_binding *new_binding = oval_variable_binding_clone(old_binding, NULL);
		oval_result_test_add_binding(new_test, new_binding);
	}
	oval_variable_binding_iterator_free(old_bindings);

	struct oval_message_iterator *old_messages = oval_result_test_get_messages(old_test);
	while (oval_message_iterator_has_more(old_messages)) {
		struct oval_message *old_message = oval_message_iterator_next(old_messages);
		struct oval_message *new_message = oval_message_clone(old_message);
		oval_result_test_add_message(new_test, new_message);
	}
	oval_message_iterator_free(old_messages);

	oval_result_test_set_instance(new_test, oval_result_test_get_instance(old_test));
	oval_result_test_set_result(new_test, old_test->result);

	return new_test;
}

struct oval_result_test *make_result_test_from_oval_test(struct oval_result_system *sys, struct oval_test *oval_test) {
	char *test_id = oval_test_get_id(oval_test);
	return oval_result_test_new(sys, test_id);
}

void oval_result_test_free(struct oval_result_test *test)
{
	__attribute__nonnull__(test);

	oval_collection_free_items(test->messages, (oscap_destruct_func) oval_message_free);
	oval_collection_free_items(test->items, (oscap_destruct_func) oval_result_item_free);
	if (test->bindings_clearable)
		oval_collection_free_items(test->bindings, (oscap_destruct_func) oval_variable_binding_free);
	else {
		oval_collection_free(test->bindings);	/* avoid leaks - remove collection but not bindings. Bindings are part of syschar model */
	}

	test->system = NULL;
	test->test = NULL;
	test->messages = NULL;
	test->result = OVAL_RESULT_NOT_EVALUATED;
	test->items = NULL;
	test->bindings = NULL;
	test->instance = 1;
	oscap_free(test);
}

bool oval_result_test_iterator_has_more(struct oval_result_test_iterator
					*oc_result_test)
{
	return oval_collection_iterator_has_more((struct oval_iterator *)
						 oc_result_test);
}

struct oval_result_test *oval_result_test_iterator_next(struct
							oval_result_test_iterator
							*oc_result_test)
{
	return (struct oval_result_test *)
	    oval_collection_iterator_next((struct oval_iterator *)
					  oc_result_test);
}

void oval_result_test_iterator_free(struct
				    oval_result_test_iterator
				    *oc_result_test)
{
	oval_collection_iterator_free((struct oval_iterator *)
				      oc_result_test);
}

struct oval_result_system *oval_result_test_get_system(struct oval_result_test *rtest)
{
	__attribute__nonnull__(rtest);

	return rtest->system;
}

struct oval_test *oval_result_test_get_test(struct oval_result_test *rtest)
{
	__attribute__nonnull__(rtest);

	return ((struct oval_result_test *)rtest)->test;
}

static int istrcmp(char *st1, char *st2)
{
	int comp_idx, ret_val;

	if ((!st1) || (!st2))
		return (1);	// if either or both is null, you stink
	for (comp_idx = 0, ret_val = 0; ((!ret_val) && (st1[comp_idx]) && (st2[comp_idx])); ++comp_idx) {
		ret_val = tolower(st2[comp_idx]) - tolower(st1[comp_idx]);
	}
	return (ret_val);
}

static oval_result_t strregcomp(char *pattern, char *test_str)
{
	int ret;
	oval_result_t result = OVAL_RESULT_ERROR;
#if defined USE_REGEX_PCRE
	pcre *re;
	const char *err;
	int errofs;

	re = pcre_compile(pattern, PCRE_UTF8, &err, &errofs, NULL);
	if (re == NULL) {
		oscap_dlprintf(DBG_E, "Unable to compile regex pattern, "
			       "pcre_compile() returned error (offset: %d): '%s'.\n", errofs, err);
		return OVAL_RESULT_ERROR;
	}

	ret = pcre_exec(re, NULL, test_str, strlen(test_str), 0, 0, NULL, 0);
	if (ret > -1 ) {
		result = OVAL_RESULT_TRUE;
	} else if (ret == -1) {
		result = OVAL_RESULT_FALSE;
	} else {
		oscap_dlprintf(DBG_E, "Unable to match regex pattern, "
			       "pcre_exec() returned error: %d.\n", ret);
		result = OVAL_RESULT_ERROR;
	}

	pcre_free(re);
#elif defined USE_REGEX_POSIX
	regex_t re;

	ret = regcomp(&re, pattern, REG_EXTENDED);
	if (ret != 0) {
		oscap_dlprintf(DBG_E, "Unable to compile regex pattern, "
			       "regcomp() returned error: %d.\n", ret);
		return OVAL_RESULT_ERROR;
	}

	ret = regexec(&re, test_str, 0, NULL, 0);
	if (ret == 0) {
		result = OVAL_RESULT_TRUE;
	} else if (ret == REG_NOMATCH) {
		result = OVAL_RESULT_FALSE;
	} else {
		oscap_dlprintf(DBG_E, "Unable to match regex pattern: %d.\n", ret);
		result = OVAL_RESULT_ERROR;
	}

	regfree(&re);
#endif
	return result;
}

static int cmp_float(float a, float b)
{
	float relative_err;
	int r;

	if (a == b)
		return 0;

	if (fabs(a) > fabs(b)) {
		r = 1;
		relative_err = fabs((a - b) / a);
	} else {
		r = -1;
		relative_err = fabs((a - b) / b);
	}
	if (relative_err <= 0.000000001)
		return 0;

	return r;
}

static oval_result_t ipv4addr_cmp(char *s1, char *s2, oval_operation_t op)
{
	oval_result_t result = OVAL_RESULT_ERROR;
	char *s, *pfx;
	int nm1, nm2;
	struct in_addr addr1, addr2;

	s = strdup(s1);
	pfx = strchr(s, '/');
	if (pfx) {
		int cnt;
		char nm[4];

		*pfx++ = '\0';
		cnt = sscanf(pfx, "%hhu.%hhu.%hhu.%hhu", &nm[0], &nm[1], &nm[2], &nm[3]);
		if (cnt > 1) { /* netmask */
			nm1 = (nm[0] << 24) + (nm[1] << 16) + (nm[2] << 8) + nm[3];
		} else { /* prefix */
			nm1 = (~0) << (32 - nm[0]);
		}
	} else {
		nm1 = ~0;
	}
	if (inet_pton(AF_INET, s, &addr1) <= 0) {
		dW("inet_pton() failed.\n");
		goto cleanup;
	}

	oscap_free(s);
	s = strdup(s2);
	pfx = strchr(s, '/');
	if (pfx) {
		int cnt;
		char nm[4];

		*pfx++ = '\0';
		cnt = sscanf(pfx, "%hhu.%hhu.%hhu.%hhu", &nm[0], &nm[1], &nm[2], &nm[3]);
		if (cnt > 1) { /* netmask */
			nm2 = (nm[0] << 24) + (nm[1] << 16) + (nm[2] << 8) + nm[3];
		} else { /* prefix */
			nm2 = (~0) << (32 - nm[0]);
		}
	} else {
		nm2 = ~0;
	}
	if (inet_pton(AF_INET, s, &addr2) <= 0) {
		dW("inet_pton() failed.\n");
		goto cleanup;
	}

	switch (op) {
	case OVAL_OPERATION_EQUALS:
		if (!memcmp(&addr1, &addr2, sizeof(struct in_addr)) && nm1 == nm2)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_NOT_EQUAL:
		if (memcmp(&addr1, &addr2, sizeof(struct in_addr)) || nm1 != nm2)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_SUBSET_OF:
		if (nm1 <= nm2) {
			result = OVAL_RESULT_FALSE;
			break;
		}
		/* FALLTHROUGH */
	case OVAL_OPERATION_GREATER_THAN:
		addr1.s_addr &= nm1;
		addr2.s_addr &= nm2;
		if (memcmp(&addr1, &addr2, sizeof(struct in_addr)) > 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_GREATER_THAN_OR_EQUAL:
		addr1.s_addr &= nm1;
		addr2.s_addr &= nm2;
		if (memcmp(&addr1, &addr2, sizeof(struct in_addr)) >= 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_SUPERSET_OF:
		if (nm1 >= nm2) {
			result = OVAL_RESULT_FALSE;
			break;
		}
		/* FALLTHROUGH */
	case OVAL_OPERATION_LESS_THAN:
		addr1.s_addr &= nm1;
		addr2.s_addr &= nm2;
		if (memcmp(&addr1, &addr2, sizeof(struct in_addr)) < 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_LESS_THAN_OR_EQUAL:
		addr1.s_addr &= nm1;
		addr2.s_addr &= nm2;
		if (memcmp(&addr1, &addr2, sizeof(struct in_addr)) <= 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	default:
		dE("Unexpected compare operation: %d.\n", op);
	}

 cleanup:
	oscap_free(s);

	return OVAL_RESULT_ERROR;
}

static void mask_v6_addrs(struct in6_addr *addr1, int p1len, struct in6_addr *addr2, int p2len)
{
	int i;

	for (i = 0; i < 128; ++i) {
		addr1->s6_addr[i / 8] &= (((i < p1len) ? 1 : 0) << (i % 8));
		addr2->s6_addr[i / 8] &= (((i < p2len) ? 1 : 0) << (i % 8));
	}
}

static oval_result_t ipv6addr_cmp(char *s1, char *s2, oval_operation_t op)
{
	oval_result_t result = OVAL_RESULT_ERROR;
	char *s, *pfx;
	int p1len, p2len;
	struct in6_addr addr1, addr2;

	s = strdup(s1);
	pfx = strchr(s, '/');
	if (pfx) {
		*pfx++ = '\0';
		p1len = strtol(pfx, NULL, 10);
	} else {
		p1len = 128;
	}
	if (inet_pton(AF_INET6, s, &addr1) <= 0) {
		dW("inet_pton() failed.\n");
		goto cleanup;
	}

	oscap_free(s);
	s = strdup(s2);
	pfx = strchr(s, '/');
	if (pfx) {
		*pfx++ = '\0';
		p2len = strtol(pfx, NULL, 10);
	} else {
		p2len = 128;
	}
	if (inet_pton(AF_INET6, s, &addr2) <= 0) {
		dW("inet_pton() failed.\n");
		goto cleanup;
	}

	switch (op) {
	case OVAL_OPERATION_EQUALS:
		if (!memcmp(&addr1, &addr2, sizeof(struct in6_addr)) && p1len == p2len)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_NOT_EQUAL:
		if (memcmp(&addr1, &addr2, sizeof(struct in6_addr)) || p1len != p2len)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_SUBSET_OF:
		if (p1len <= p2len) {
			result = OVAL_RESULT_FALSE;
			break;
		}
		/* FALLTHROUGH */
	case OVAL_OPERATION_GREATER_THAN:
		mask_v6_addrs(&addr1, p1len, &addr2, p2len);
		if (memcmp(&addr1, &addr2, sizeof(struct in6_addr)) > 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_GREATER_THAN_OR_EQUAL:
		mask_v6_addrs(&addr1, p1len, &addr2, p2len);
		if (memcmp(&addr1, &addr2, sizeof(struct in6_addr)) >= 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_SUPERSET_OF:
		if (p1len >= p2len) {
			result = OVAL_RESULT_FALSE;
			break;
		}
		/* FALLTHROUGH */
	case OVAL_OPERATION_LESS_THAN:
		mask_v6_addrs(&addr1, p1len, &addr2, p2len);
		if (memcmp(&addr1, &addr2, sizeof(struct in6_addr)) < 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	case OVAL_OPERATION_LESS_THAN_OR_EQUAL:
		mask_v6_addrs(&addr1, p1len, &addr2, p2len);
		if (memcmp(&addr1, &addr2, sizeof(struct in6_addr)) <= 0)
			result = OVAL_RESULT_TRUE;
		else
			result = OVAL_RESULT_FALSE;
		break;
	default:
		dE("Unexpected compare operation: %d.\n", op);
	}

 cleanup:
	oscap_free(s);

	return result;
}

// finally, we have gotten to the point of comparing system data with a state
static oval_result_t evaluate(char *sys_data, char *state_data, oval_datatype_t sys_data_type,
			      oval_datatype_t state_data_type, oval_operation_t operation)
{
	// todo: cast values to a common type

	if (state_data_type == OVAL_DATATYPE_STRING) {
		if (operation == OVAL_OPERATION_EQUALS) {
			return ((oscap_strcmp(state_data, sys_data)) ? OVAL_RESULT_FALSE : OVAL_RESULT_TRUE);
		} else if (operation == OVAL_OPERATION_CASE_INSENSITIVE_EQUALS) {
			return ((istrcmp(state_data, sys_data)) ? OVAL_RESULT_FALSE : OVAL_RESULT_TRUE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return ((oscap_strcmp(state_data, sys_data)) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_CASE_INSENSITIVE_NOT_EQUAL) {
			return ((istrcmp(state_data, sys_data)) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_PATTERN_MATCH) {
			return strregcomp(state_data, sys_data);
		} else {
			oscap_dlprintf(DBG_E, "Invalid type of operation in string evaluation: %d.\n", operation);
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in string evaluation");
			return OVAL_RESULT_ERROR;
		}
	} else if (state_data_type == OVAL_DATATYPE_INTEGER) {
		int state_val, syschar_val;
		state_val = atoi(state_data);
		syschar_val = atoi(sys_data);
		if (operation == OVAL_OPERATION_EQUALS) {
			return ((state_val == syschar_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return ((state_val != syschar_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN) {
			return ((syschar_val > state_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN_OR_EQUAL) {
			return ((syschar_val >= state_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN) {
			return ((syschar_val < state_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN_OR_EQUAL) {
			return ((syschar_val <= state_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_BITWISE_AND) {
			return (((syschar_val & state_val) == state_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_BITWISE_OR) {
			return (((syschar_val | state_val) == state_val) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else {
			oscap_dlprintf(DBG_E, "Invalid type of operation in integer evaluation: %d.\n", operation);
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in integer evaluation");
			return OVAL_RESULT_ERROR;
		}
	} else if (state_data_type == OVAL_DATATYPE_FLOAT) {
		float state_val, sys_val;

		state_val = atof(state_data);
		sys_val = atof(sys_data);
		if (operation == OVAL_OPERATION_EQUALS) {
			return ((cmp_float(sys_val, state_val) == 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return ((cmp_float(sys_val, state_val) != 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN) {
			return ((cmp_float(sys_val, state_val) == 1) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN_OR_EQUAL) {
			return ((cmp_float(sys_val, state_val) >= 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN) {
			return ((cmp_float(sys_val, state_val) == -1) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN_OR_EQUAL) {
			return ((cmp_float(sys_val, state_val) <= 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else {
			oscap_dlprintf(DBG_E, "Invalid type of operation in float evaluation: %s.\n", oval_operation_get_text(operation));
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in float evaluation");
			return OVAL_RESULT_ERROR;
		}
	} else if (state_data_type == OVAL_DATATYPE_BOOLEAN) {
		int state_int;
		int sys_int;
		state_int = (((strcmp(state_data, "true")) == 0) || ((strcmp(state_data, "1")) == 0)) ? 1 : 0;
		sys_int = (((strcmp(sys_data, "true")) == 0) || ((strcmp(sys_data, "1")) == 0)) ? 1 : 0;
		if (operation == OVAL_OPERATION_EQUALS) {
			return ((state_int == sys_int) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return ((state_int != sys_int) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else {
			oscap_dlprintf(DBG_E, "Invalid type of operation in boolean evaluation: %d.\n", operation);
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in boolean evaluation");
			return OVAL_RESULT_ERROR;
		}
	} else if (state_data_type == OVAL_DATATYPE_BINARY) {	// I'm going to use case insensitive compare here - don't know if it's necessary
		if (operation == OVAL_OPERATION_EQUALS) {
			return ((istrcmp(state_data, sys_data) == 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return ((istrcmp(state_data, sys_data) != 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else {
			oscap_dlprintf(DBG_E, "Invalid type of operation in binary evaluation: %d.\n", operation);
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in binary evaluation");
			return OVAL_RESULT_ERROR;
		}
	} else if (state_data_type == OVAL_DATATYPE_EVR_STRING) {
		int result;
		result = rpmvercmp(sys_data, state_data);
		if (operation == OVAL_OPERATION_EQUALS) {
			return ((result == 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return ((result != 0) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN) {
			return ((result == 1) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN_OR_EQUAL) {
			return ((result != -1) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN) {
			return ((result == -1) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN_OR_EQUAL) {
			return ((result != 1) ? OVAL_RESULT_TRUE : OVAL_RESULT_FALSE);
		} else {
			oscap_dlprintf(DBG_E, "Invalid type of operation in rpm version comparison: %d.\n", operation);
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in rpm version comparison");
			return OVAL_RESULT_ERROR;
		}
	} else if (state_data_type == OVAL_DATATYPE_VERSION) {
		int state_idx = 0;
		int sys_idx = 0;
		int result = -1;
		/* int is_equal = 1; */
		for (state_idx = 0, sys_idx = 0; (((state_data[state_idx]) || (sys_data[sys_idx])) && (result == -1));) {	// keep going as long as there is data in either the state or sysitem
			int tmp_state_int, tmp_sys_int;
			tmp_state_int = atoi(&state_data[state_idx]);	// look at the current data field (if we're at the end, atoi should return 0)
			tmp_sys_int = atoi(&sys_data[sys_idx]);
                        /* o rly?
			if (tmp_state_int != tmp_sys_int)
				is_equal = 0;	// we might need this later (if we don't terminate early)
                        */
			if (operation == OVAL_OPERATION_EQUALS) {
				if (tmp_state_int != tmp_sys_int)
					return (OVAL_RESULT_FALSE);
			} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
				if (tmp_state_int != tmp_sys_int)
					return (OVAL_RESULT_TRUE);
			} else if ((operation == OVAL_OPERATION_GREATER_THAN)
				   || (operation == OVAL_OPERATION_GREATER_THAN_OR_EQUAL)) {
				if (tmp_sys_int > tmp_state_int)
					return (OVAL_RESULT_TRUE);
				if (tmp_sys_int < tmp_state_int)
					return (OVAL_RESULT_FALSE);
			} else if ((operation == OVAL_OPERATION_LESS_THAN)
				   || (operation == OVAL_OPERATION_LESS_THAN_OR_EQUAL)) {
				if (tmp_sys_int < tmp_state_int)
					return (OVAL_RESULT_TRUE);
				if (tmp_sys_int > tmp_state_int)
					return (OVAL_RESULT_FALSE);
			} else {
				oscap_dlprintf(DBG_E, "Invalid type of operation in version comparison: %d.\n", operation);
				oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Invalid type of operation in version comparison");
				return OVAL_RESULT_ERROR;
			}

			if (state_data[state_idx])
				++state_idx;
			/* move to the next field within the version string (if there is one) */
			while ((state_data[state_idx]) && (isdigit(state_data[state_idx])))
				++state_idx;
			if ((state_data[state_idx]) && (!isdigit(state_data[state_idx])))
				++state_idx;

			if (sys_data[sys_idx])
				++sys_idx;
			/* move to the next field within the version string (if there is one) */
			while ((sys_data[sys_idx]) && (isdigit(sys_data[sys_idx])))
				++sys_idx;
			if ((sys_data[sys_idx]) && (!isdigit(sys_data[sys_idx])))
				++sys_idx;
		}
		// OK, we did not terminate early, and we're out of data, so we now know what to return
		if (operation == OVAL_OPERATION_EQUALS) {
			return (OVAL_RESULT_TRUE);
		} else if (operation == OVAL_OPERATION_NOT_EQUAL) {
			return (OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN) {
			return (OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_GREATER_THAN_OR_EQUAL) {
			return (OVAL_RESULT_TRUE);
		} else if (operation == OVAL_OPERATION_LESS_THAN) {
			return (OVAL_RESULT_FALSE);
		} else if (operation == OVAL_OPERATION_LESS_THAN_OR_EQUAL) {
			return (OVAL_RESULT_TRUE);
		}		// we have already filtered out the invalid ones
	} else if (state_data_type == OVAL_DATATYPE_IPV4ADDR) {
		return ipv4addr_cmp(state_data, sys_data, operation);
	} else if (state_data_type == OVAL_DATATYPE_IPV6ADDR) {
		return ipv6addr_cmp(state_data, sys_data, operation);
	} else if (state_data_type == OVAL_DATATYPE_FILESET_REVISION
		   || state_data_type == OVAL_DATATYPE_IOS_VERSION) {
		dW("Unsupported data type: %s.\n", oval_datatype_get_text(state_data_type));
		return OVAL_RESULT_NOT_EVALUATED;
	}

        oscap_dlprintf(DBG_E, "Ivalid OVAL data type: %d.\n", state_data_type);
        oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Ivalid OVAL data type");
        return OVAL_RESULT_ERROR;
}

int ores_add_res(struct oresults *ores, oval_result_t res)
{
		switch (res) {
		case OVAL_RESULT_TRUE:
			ores->true_cnt++;
			break;
		case OVAL_RESULT_FALSE:
			ores->false_cnt++;
			break;
		case OVAL_RESULT_UNKNOWN:
			ores->unknown_cnt++;
			break;
		case OVAL_RESULT_ERROR:
			ores->error_cnt++;
			break;
		case OVAL_RESULT_NOT_EVALUATED:
			ores->noteval_cnt++;
			break;
		case OVAL_RESULT_NOT_APPLICABLE:
			ores->notappl_cnt++;
			break;
		default:
	                oscap_dlprintf(DBG_E, "Invalid oval result type: %d.\n", res);
        	        oscap_seterr(OSCAP_EFAMILY_OSCAP, OVAL_EOVALINT, "Invalid oval result type");
			return 1;
		}

		return 0;
}

void ores_clear(struct oresults *ores)
{
	memset(ores, 0, sizeof (*ores));
}

oval_result_t ores_get_result_bychk(struct oresults *ores, oval_check_t check)
{
	oval_result_t result = OVAL_RESULT_ERROR;

	if (ores->true_cnt == 0 &&
	    ores->false_cnt == 0 &&
	    ores->error_cnt == 0 &&
	    ores->unknown_cnt == 0 &&
	    ores->notappl_cnt == 0 &&
	    ores->noteval_cnt == 0)
		return OVAL_RESULT_UNKNOWN;

	if (ores->notappl_cnt > 0 &&
	    ores->noteval_cnt == 0 &&
	    ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->true_cnt == 0)
		return OVAL_RESULT_NOT_APPLICABLE;

	switch (check) {
	case OVAL_CHECK_ALL:
		if (ores->true_cnt > 0 &&
		    ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (ores->false_cnt > 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->false_cnt == 0 && ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		}
		break;
	case OVAL_CHECK_AT_LEAST_ONE:
		if (ores->true_cnt > 0) {
			result = OVAL_RESULT_TRUE;
		} else if (ores->false_cnt > 0 &&
			   ores->true_cnt == 0 &&
			   ores->unknown_cnt == 0 && ores->error_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->true_cnt == 0 && ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		}
		break;
	case OVAL_CHECK_NONE_EXIST:
		dW("The 'none exist' CheckEnumeration value has been deprecated. "
		   "Converted to check='none satisfy'.\n");
		/* FALLTHROUGH */
	case OVAL_CHECK_NONE_SATISFY:
		if (ores->true_cnt > 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->true_cnt == 0 && ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->true_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->true_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		} else if (ores->false_cnt > 0 &&
			   ores->error_cnt == 0 &&
			   ores->unknown_cnt == 0 && ores->noteval_cnt == 0 && ores->true_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		}
		break;
	case OVAL_CHECK_ONLY_ONE:
		if (ores->true_cnt == 1 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (ores->true_cnt > 1) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->true_cnt < 2 && ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->true_cnt < 2 && ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->true_cnt < 2 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		} else if (ores->true_cnt != 1 && ores->false_cnt > 0) {
			result = OVAL_RESULT_FALSE;
		}
		break;
	default:
                oscap_dlprintf(DBG_E, "Invalid check value: %d.\n", check);
                oscap_seterr(OSCAP_EFAMILY_OSCAP, OVAL_EOVALINT, "Invalid check value");
		result = OVAL_RESULT_ERROR;
	}

	return result;
}

oval_result_t ores_get_result_byopr(struct oresults *ores, oval_operator_t op)
{
	oval_result_t result = OVAL_RESULT_ERROR;

	if (ores->true_cnt == 0 &&
	    ores->false_cnt == 0 &&
	    ores->error_cnt == 0 &&
	    ores->unknown_cnt == 0 &&
	    ores->notappl_cnt == 0 &&
	    ores->noteval_cnt == 0)
		return OVAL_RESULT_UNKNOWN;

	if (ores->notappl_cnt > 0 &&
	    ores->noteval_cnt == 0 &&
	    ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->true_cnt == 0)
		return OVAL_RESULT_NOT_APPLICABLE;

	switch (op) {
	case OVAL_OPERATOR_AND:
		if (ores->true_cnt > 0 &&
		    ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (ores->false_cnt > 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->false_cnt == 0 && ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->false_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		}
		break;
	case OVAL_OPERATOR_ONE:
		if (ores->true_cnt == 1 &&
		    ores->false_cnt >= 0 &&
		    ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0 && ores->notappl_cnt >= 0) {
			result = OVAL_RESULT_TRUE;
		} else if (ores->true_cnt >= 2 &&
			   ores->false_cnt >= 0 &&
			   ores->error_cnt >= 0 &&
			   ores->unknown_cnt >= 0 && ores->noteval_cnt >= 0 && ores->notappl_cnt >= 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->true_cnt == 0 &&
			   ores->false_cnt >= 0 &&
			   ores->error_cnt == 0 &&
			   ores->unknown_cnt == 0 && ores->noteval_cnt == 0 && ores->notappl_cnt >= 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->true_cnt < 2 &&
			   ores->false_cnt >= 0 &&
			   ores->error_cnt > 0 &&
			   ores->unknown_cnt >= 0 && ores->noteval_cnt >= 0 && ores->notappl_cnt >= 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->true_cnt < 2 &&
			   ores->false_cnt >= 0 &&
			   ores->error_cnt == 0 &&
			   ores->unknown_cnt >= 1 && ores->noteval_cnt >= 0 && ores->notappl_cnt >= 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->true_cnt < 2 &&
			   ores->false_cnt >= 0 &&
			   ores->error_cnt == 0 &&
			   ores->unknown_cnt == 0 && ores->noteval_cnt > 0 && ores->notappl_cnt >= 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		}
		break;
	case OVAL_OPERATOR_OR:
		if (ores->true_cnt > 0) {
			result = OVAL_RESULT_TRUE;
		} else if (ores->true_cnt == 0 &&
			   ores->false_cnt > 0 &&
			   ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->true_cnt == 0 && ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->true_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->true_cnt == 0 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		}
		break;
	case OVAL_OPERATOR_XOR:
		if ((ores->true_cnt % 2) == 1 && ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if ((ores->true_cnt % 2) == 0 &&
			   ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (ores->error_cnt > 0) {
			result = OVAL_RESULT_ERROR;
		} else if (ores->error_cnt == 0 && ores->unknown_cnt > 0) {
			result = OVAL_RESULT_UNKNOWN;
		} else if (ores->error_cnt == 0 && ores->unknown_cnt == 0 && ores->noteval_cnt > 0) {
			result = OVAL_RESULT_NOT_EVALUATED;
		}
		break;
	default:
                oscap_dlprintf(DBG_E, "Invalid operator value: %d.\n", op);
                oscap_seterr(OSCAP_EFAMILY_OSCAP, OVAL_EOVALINT, "Invalid operator value");
		result = OVAL_RESULT_ERROR;
		break;
	}

	return result;
}

static oval_result_t eval_item(struct oval_syschar_model *syschar_model, struct oval_sysitem *cur_sysitem, struct oval_state *state)
{
	struct oval_state_content_iterator *state_contents_itr;
	struct oresults ste_ores;
	oval_operator_t operator;
	oval_result_t result = OVAL_RESULT_ERROR;

	ores_clear(&ste_ores);

	state_contents_itr = oval_state_get_contents(state);
	while (oval_state_content_iterator_has_more(state_contents_itr)) {
		struct oval_state_content *content;
		struct oval_entity *state_entity;
		char *state_entity_name;
		struct oval_value *state_entity_val;
		char *state_entity_val_text = NULL;
		oval_datatype_t state_entity_val_datatype = OVAL_DATATYPE_UNKNOWN;
		oval_operation_t state_entity_operation;
		oval_check_t entity_check;
		struct oval_variable *state_entity_var;
		oval_check_t var_check = OVAL_CHECK_UNKNOWN;
		oval_result_t ste_ent_res;
		struct oval_sysent_iterator *item_entities_itr;
		struct oresults ent_ores;
		bool found_matching_item;

		if ((content = oval_state_content_iterator_next(state_contents_itr)) == NULL) {
			oscap_dlprintf(DBG_E, "Found NULL state content.\n");
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
				     "OVAL internal error: found NULL state content");
			goto fail;
		}
		if ((state_entity = oval_state_content_get_entity(content)) == NULL) {
			oscap_dlprintf(DBG_E, "Found NULL entity.\n");
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
				     "OVAL internal error: found NULL entity");
			goto fail;
		}
		if ((state_entity_name = oval_entity_get_name(state_entity)) == NULL) {
			oscap_dlprintf(DBG_E, "Found NULL entity name.\n");
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
				     "OVAL internal error: found NULL entity name");
			goto fail;
		}

		entity_check = oval_state_content_get_ent_check(content);
		state_entity_operation = oval_entity_get_operation(state_entity);

		if (oval_entity_get_varref_type(state_entity) == OVAL_ENTITY_VARREF_ATTRIBUTE) {
			if ((state_entity_var = oval_entity_get_variable(state_entity)) == NULL) {
				oscap_dlprintf(DBG_E, "Found NULL variable.\n");
				oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
					     "OVAL internal error: found NULL variable");
				goto fail;
			}
			var_check = oval_state_content_get_var_check(content);
		} else {
			state_entity_var = NULL;

			if ((state_entity_val = oval_entity_get_value(state_entity)) == NULL) {
				oscap_dlprintf(DBG_E, "Found NULL entity value.\n");
				oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
					     "OVAL internal error: found NULL entity value");
				goto fail;
			}
			if ((state_entity_val_text = oval_value_get_text(state_entity_val)) == NULL) {
				oscap_dlprintf(DBG_E, "Found NULL entity value text.\n");
				oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
					     "OVAL internal error: found NULL entity value text");
				goto fail;
			}
			state_entity_val_datatype = oval_value_get_datatype(state_entity_val);
		}

		ores_clear(&ent_ores);
		found_matching_item = false;

		item_entities_itr = oval_sysitem_get_sysents(cur_sysitem);
		while (oval_sysent_iterator_has_more(item_entities_itr)) {
			struct oval_sysent *item_entity;
			oval_result_t ent_val_res;
			char *item_entity_name;

			item_entity = oval_sysent_iterator_next(item_entities_itr);
			if (item_entity == NULL) {
				oscap_dlprintf(DBG_E, "Found NULL sysent.\n");
				oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT,
					     "OVAL internal error: found NULL sysent");
				oval_sysent_iterator_free(item_entities_itr);
				goto fail;
			}

			item_entity_name = oval_sysent_get_name(item_entity);
			if (strcmp(item_entity_name, state_entity_name))
				continue;

			found_matching_item = true;

			/* copy mask attribute from state to item */
			if (oval_entity_get_mask(state_entity))
				oval_sysent_set_mask(item_entity,1);

			if (oval_sysent_get_status(item_entity) == SYSCHAR_STATUS_DOES_NOT_EXIST) {
				ent_val_res = OVAL_RESULT_FALSE;
			} else if (state_entity_var != NULL) {
				struct oresults var_ores;
				struct oval_value_iterator *val_itr;
				oval_syschar_collection_flag_t flag;

				if (0 != oval_syschar_model_compute_variable(syschar_model, state_entity_var)) {
					oval_sysent_iterator_free(item_entities_itr);
					goto fail;
				}

				flag = oval_variable_get_collection_flag(state_entity_var);
				switch (flag) {
				case SYSCHAR_FLAG_COMPLETE:
				case SYSCHAR_FLAG_INCOMPLETE:
					ores_clear(&var_ores);

					val_itr = oval_variable_get_values(state_entity_var);
					while (oval_value_iterator_has_more(val_itr)) {
						struct oval_value *var_val;
						oval_result_t var_val_res;

						var_val = oval_value_iterator_next(val_itr);
						state_entity_val_text = oval_value_get_text(var_val);
						if (state_entity_val_text == NULL) {
							dE("Found NULL variable value text.\n");
							ores_add_res(&var_ores, OVAL_RESULT_ERROR);
							break;
						}
						state_entity_val_datatype = oval_value_get_datatype(var_val);

						var_val_res = evaluate(oval_sysent_get_value(item_entity),
								       state_entity_val_text,
								       oval_sysent_get_datatype(item_entity),
								       state_entity_val_datatype,
								       state_entity_operation);
						ores_add_res(&var_ores, var_val_res);
					}
					oval_value_iterator_free(val_itr);

					ent_val_res = ores_get_result_bychk(&var_ores, var_check);
					break;
				case SYSCHAR_FLAG_ERROR:
				case SYSCHAR_FLAG_DOES_NOT_EXIST:
				case SYSCHAR_FLAG_NOT_COLLECTED:
				case SYSCHAR_FLAG_NOT_APPLICABLE:
					ent_val_res = OVAL_RESULT_ERROR;
					break;
				default:
					oval_sysent_iterator_free(item_entities_itr);
					goto fail;
				}

			} else {
				ent_val_res = evaluate(oval_sysent_get_value(item_entity),
						       state_entity_val_text,
						       oval_sysent_get_datatype(item_entity),
						       state_entity_val_datatype,
						       state_entity_operation);
			}
			ores_add_res(&ent_ores, ent_val_res);
		}
		oval_sysent_iterator_free(item_entities_itr);

		if (!found_matching_item)
			dW("Entity name '%s' from state (id: '%s') not found in item (id: '%s').\n",
			   state_entity_name, oval_state_get_id(state), oval_sysitem_get_id(cur_sysitem));

		ste_ent_res = ores_get_result_bychk(&ent_ores, entity_check);
		ores_add_res(&ste_ores, ste_ent_res);
	}
	oval_state_content_iterator_free(state_contents_itr);

	operator = oval_state_get_operator(state);
	result = ores_get_result_byopr(&ste_ores, operator);

	return result;

 fail:
	oval_state_content_iterator_free(state_contents_itr);

	return OVAL_RESULT_ERROR;
}

#define ITEMMAP (struct oval_string_map    *)args[2]
#define TEST    (struct oval_result_test   *)args[1]
#define SYSTEM  (struct oval_result_system *)args[0]

static void _oval_test_item_consumer(struct oval_result_item *item, void **args) {
	struct oval_sysitem *oval_sysitem = oval_result_item_get_sysitem(item);
	char *item_id = oval_sysitem_get_id(oval_sysitem);
	struct oval_result_item *mapped_item = oval_string_map_get_value(ITEMMAP, item_id);
	if (mapped_item == NULL) {
		oval_string_map_put(ITEMMAP, item_id, item);
		oval_result_test_add_item(TEST, item);
	} else {
		oval_result_item_free(item);
	}
}

static oval_result_t eval_check_state(struct oval_test *test, void **args)
{
	struct oval_syschar_model *syschar_model;
	struct oval_result_item_iterator *ritems_itr;
	struct oresults item_ores;
	oval_result_t result;
	oval_check_t ste_check;
	oval_operator_t ste_opr;

	ste_check = oval_test_get_check(test);
	ste_opr = oval_test_get_state_operator(test);
	syschar_model = oval_result_system_get_syschar_model(SYSTEM);
	ores_clear(&item_ores);

	ritems_itr = oval_result_test_get_items(TEST);
	while (oval_result_item_iterator_has_more(ritems_itr)) {
		struct oval_result_item *ritem;
		struct oval_sysitem *item;
		oval_syschar_status_t item_status;
		struct oresults ste_ores;
		struct oval_state_iterator *ste_itr;
		oval_result_t item_res;

		ritem = oval_result_item_iterator_next(ritems_itr);
		item = oval_result_item_get_sysitem(ritem);

		item_status = oval_sysitem_get_status(item);
		switch (item_status) {
		case SYSCHAR_STATUS_ERROR:
		case SYSCHAR_STATUS_NOT_COLLECTED:
			item_res = OVAL_RESULT_ERROR;
			ores_add_res(&item_ores, item_res);
			oval_result_item_set_result(ritem, item_res);
			continue;
		case SYSCHAR_STATUS_DOES_NOT_EXIST:
			item_res = OVAL_RESULT_FALSE;
			ores_add_res(&item_ores, item_res);
			oval_result_item_set_result(ritem, item_res);
			continue;
		default:
			break;
		}

		ores_clear(&ste_ores);

		ste_itr = oval_test_get_states(test);
		while (oval_state_iterator_has_more(ste_itr)) {
			struct oval_state *ste;
			oval_result_t ste_res;

			ste = oval_state_iterator_next(ste_itr);
			ste_res = eval_item(syschar_model, item, ste);
			ores_add_res(&ste_ores, ste_res);
		}
		oval_state_iterator_free(ste_itr);

		item_res = ores_get_result_byopr(&ste_ores, ste_opr);
		ores_add_res(&item_ores, item_res);
		oval_result_item_set_result(ritem, item_res);
	}
	oval_result_item_iterator_free(ritems_itr);

	result = ores_get_result_bychk(&item_ores, ste_check);

	return result;
}

static oval_result_t eval_check_existence(oval_existence_t check_existence, int exists_cnt, int error_cnt)
{
	oval_result_t result = OVAL_RESULT_ERROR;

	switch (check_existence) {
	case OVAL_ALL_EXIST:
		if (exists_cnt >= 1 && error_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (exists_cnt == 0 && error_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (exists_cnt >= 0 && error_cnt >= 1) {
			result = OVAL_RESULT_ERROR;
		}
		break;
	case OVAL_ANY_EXIST:
		if (exists_cnt >= 0 && error_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (exists_cnt >= 1 && error_cnt >= 1) {
			result = OVAL_RESULT_TRUE;
		} else if (exists_cnt == 0 && error_cnt >= 1) {
			result = OVAL_RESULT_ERROR;
		}
		break;
	case OVAL_AT_LEAST_ONE_EXISTS:
		if (exists_cnt >= 1 && error_cnt >= 0) {
			result = OVAL_RESULT_TRUE;
		} else if (exists_cnt == 0 && error_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (exists_cnt == 0 && error_cnt >= 1) {
			result = OVAL_RESULT_ERROR;
		}
		break;
	case OVAL_NONE_EXIST:
		if (exists_cnt == 0 && error_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (exists_cnt >= 1 && error_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (exists_cnt == 0 && error_cnt >= 1) {
			result = OVAL_RESULT_ERROR;
		}
		break;
	case OVAL_ONLY_ONE_EXISTS:
		if (exists_cnt == 1 && error_cnt == 0) {
			result = OVAL_RESULT_TRUE;
		} else if (exists_cnt >= 2 && error_cnt >= 0) {
			result = OVAL_RESULT_FALSE;
		} else if (exists_cnt == 0 && error_cnt == 0) {
			result = OVAL_RESULT_FALSE;
		} else if (exists_cnt == 0 && error_cnt >= 1) {
			result = OVAL_RESULT_ERROR;
		} else if (exists_cnt == 1 && error_cnt >= 1) {
			result = OVAL_RESULT_ERROR;
		}
		break;
	default:
                oscap_dlprintf(DBG_E, "Invalid check_existence value: %d.\n", check_existence);
                oscap_seterr(OSCAP_EFAMILY_OSCAP, OVAL_EOVALINT, "Invalid check_existence value");
                result = OVAL_RESULT_ERROR;
		break;
	}

	return result;
}

static oval_result_t
_oval_result_test_evaluate_items(struct oval_test *test, struct oval_syschar *syschar_object, void **args)
{
	struct oval_sysitem_iterator *collected_items_itr;
	oval_result_t result;
	int exists_cnt, error_cnt;
	bool hasstate;
	oval_check_t test_check;
	oval_existence_t test_check_existence;
	struct oval_state_iterator *ste_itr;

	exists_cnt = error_cnt = 0;
	collected_items_itr = oval_syschar_get_sysitem(syschar_object);
	while (oval_sysitem_iterator_has_more(collected_items_itr)) {
		struct oval_sysitem *item;
		char *item_id;
		oval_syschar_status_t item_status;
		struct oval_result_item *ritem;

		item = oval_sysitem_iterator_next(collected_items_itr);
		if (item == NULL) {
			oscap_dlprintf(DBG_E, "Iterator returned null.");
			oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Iterator returned null");
			oval_sysitem_iterator_free(collected_items_itr);
			return OVAL_RESULT_ERROR;
		}

		item_status = oval_sysitem_get_status(item);
		if (item_status == SYSCHAR_STATUS_EXISTS)
			exists_cnt++;
		if (item_status == SYSCHAR_STATUS_ERROR)
			error_cnt++;

		item_id = oval_sysitem_get_id(item);
		ritem = oval_result_item_new(SYSTEM, item_id);
		oval_result_item_set_result(ritem, OVAL_RESULT_NOT_EVALUATED);
		_oval_test_item_consumer(ritem, args);
	}
	oval_sysitem_iterator_free(collected_items_itr);

	test_check = oval_test_get_check(test);
	test_check_existence = oval_test_get_existence(test);
	ste_itr = oval_test_get_states(test);
	hasstate = oval_state_iterator_has_more(ste_itr);
	oval_state_iterator_free(ste_itr);

	switch (oval_syschar_get_flag(syschar_object)) {
	case SYSCHAR_FLAG_ERROR:
		if (test_check_existence == OVAL_ANY_EXIST
		    && !hasstate) {
			result = OVAL_RESULT_TRUE;
		} else {
			result = OVAL_RESULT_ERROR;
		}
		break;
	case SYSCHAR_FLAG_NOT_COLLECTED:
		if (test_check_existence == OVAL_ANY_EXIST
		    && !hasstate) {
			result = OVAL_RESULT_TRUE;
		} else {
			result = OVAL_RESULT_UNKNOWN;
		}
		break;
	case SYSCHAR_FLAG_NOT_APPLICABLE:
		if (test_check_existence == OVAL_ANY_EXIST
		    && !hasstate) {
			result = OVAL_RESULT_TRUE;
		} else {
			result = OVAL_RESULT_NOT_APPLICABLE;
		}
		break;
	case SYSCHAR_FLAG_DOES_NOT_EXIST:
		if (test_check_existence == OVAL_NONE_EXIST
		    || test_check_existence == OVAL_ANY_EXIST) {
			result = OVAL_RESULT_TRUE;
		} else {
			result = OVAL_RESULT_FALSE;
		}
		break;
	case SYSCHAR_FLAG_COMPLETE:
		result = eval_check_existence(test_check_existence, exists_cnt, error_cnt);
		if (result == OVAL_RESULT_TRUE
		    && hasstate) {
			result = eval_check_state(test, args);
		}
		break;
	case SYSCHAR_FLAG_INCOMPLETE:
		if (test_check_existence == OVAL_ANY_EXIST) {
			result = OVAL_RESULT_TRUE;
		} else if (test_check_existence == OVAL_AT_LEAST_ONE_EXISTS
			   && exists_cnt > 0) {
			result = OVAL_RESULT_TRUE;
		} else if (test_check_existence == OVAL_NONE_EXIST
			   && exists_cnt > 0) {
			result = OVAL_RESULT_FALSE;
		} else if (test_check_existence == OVAL_ONLY_ONE_EXISTS
			   && exists_cnt > 1) {
			result = OVAL_RESULT_FALSE;
		} else {
			result = OVAL_RESULT_UNKNOWN;
		}

		if (result == OVAL_RESULT_TRUE
		    && hasstate) {
			result = eval_check_state(test, args);
			if (result == OVAL_RESULT_TRUE) {
				if (test_check != OVAL_CHECK_AT_LEAST_ONE) {
					result = OVAL_RESULT_UNKNOWN;
				}
			} else if (result != OVAL_RESULT_FALSE) {
				result = OVAL_RESULT_UNKNOWN;
			}
		}
		break;
	default:
		oscap_dlprintf(DBG_E, "Unknown syschar flag: %d.\n", oval_syschar_get_flag(syschar_object));
		oscap_seterr(OSCAP_EFAMILY_OVAL, OVAL_EOVALINT, "Unknown syschar flag");
		return OVAL_RESULT_ERROR;
	}

	return result;
}

/* this function will gather all the necessary ingredients and call 'evaluate_items' when it finds them */
static oval_result_t _oval_result_test_result(struct oval_result_test *rtest, void **args)
{
	__attribute__nonnull__(rtest);

	/* is the test already evaluated? */
	if (rtest->result != OVAL_RESULT_NOT_EVALUATED) {
		dI("Found result from previous evaluation: %d, returning without further processing.\n", rtest->result);
		return (rtest->result);
	}

	/* get syschar of rtest */
	struct oval_test *test = oval_result_test_get_test(rtest);
	struct oval_object * object = oval_test_get_object(test);
	char * object_id = oval_object_get_id(object);

	struct oval_result_system *sys = oval_result_test_get_system(rtest);
	struct oval_syschar_model *syschar_model = oval_result_system_get_syschar_model(sys);

	struct oval_syschar * syschar = oval_syschar_model_get_syschar(syschar_model, object_id);
	if (syschar == NULL) {
		dW("No syschar for object: %s\n", object_id);
		return OVAL_RESULT_UNKNOWN;
	}

	/* evaluate items */
	oval_result_t result = _oval_result_test_evaluate_items(test, syschar, args);

	return result;
}

static void _oval_result_test_initialize_bindings(struct oval_result_test *rslt_test)
{
	__attribute__nonnull__(rslt_test);

	struct oval_test *oval_test = oval_result_test_get_test(rslt_test);

	struct oval_object *oval_object = oval_test_get_object(oval_test);
	if (oval_object) {
		char *object_id = oval_object_get_id(oval_object);
		struct oval_result_system *sys = oval_result_test_get_system(rslt_test);
		struct oval_syschar_model *syschar_model = oval_result_system_get_syschar_model(sys);
		struct oval_syschar *syschar = oval_syschar_model_get_syschar(syschar_model, object_id);
		/* no syschar if system characteristics was a subset of definitions */
		if(syschar) {
			struct oval_variable_binding_iterator *bindings = oval_syschar_get_variable_bindings(syschar);
			while (oval_variable_binding_iterator_has_more(bindings)) {
				struct oval_variable_binding *binding = oval_variable_binding_iterator_next(bindings);
				oval_result_test_add_binding(rslt_test, binding);
			}
			oval_variable_binding_iterator_free(bindings);
		}
	}
	rslt_test->bindings_initialized = true;
	rslt_test->bindings_clearable = false;	//bindings are shared from syschar model.
}

oval_result_t oval_result_test_eval(struct oval_result_test *rtest)
{
	__attribute__nonnull__(rtest);

	if (rtest->result == OVAL_RESULT_NOT_EVALUATED) {
		if ((oval_independent_subtype_t)oval_test_get_subtype(oval_result_test_get_test(rtest)) != OVAL_INDEPENDENT_UNKNOWN ) {
			struct oval_string_map *tmp_map = oval_string_map_new();
			void *args[] = { rtest->system, rtest, tmp_map };
			rtest->result = _oval_result_test_result(rtest, args);
			oval_string_map_free(tmp_map, NULL);

			if (!rtest->bindings_initialized) {
				_oval_result_test_initialize_bindings(rtest);
			}
		}
		else
			rtest->result = OVAL_RESULT_UNKNOWN;
	}

        dI("\t%s => %s\n", oval_test_get_id(oval_result_test_get_test(rtest)), oval_result_get_text(rtest->result));

	return rtest->result;
}

oval_result_t oval_result_test_get_result(struct oval_result_test * rtest)
{
	__attribute__nonnull__(rtest);

	return rtest->result;
}

int oval_result_test_get_instance(struct oval_result_test *rtest)
{
	__attribute__nonnull__(rtest);

	return rtest->instance;
}

struct oval_message_iterator *oval_result_test_get_messages(struct oval_result_test *rtest)
{
	__attribute__nonnull__(rtest);

	return (struct oval_message_iterator *) oval_collection_iterator(rtest->messages);
}

struct oval_result_item_iterator *oval_result_test_get_items(struct oval_result_test *rtest)
{
	__attribute__nonnull__(rtest);

	return (struct oval_result_item_iterator *)
	    oval_collection_iterator(rtest->items);
}

struct oval_variable_binding_iterator *oval_result_test_get_bindings(struct oval_result_test
								     *rtest)
{
	__attribute__nonnull__(rtest);

	return (struct oval_variable_binding_iterator *)
	    oval_collection_iterator(rtest->bindings);
}

void oval_result_test_set_result(struct oval_result_test *test, oval_result_t result)
{
	__attribute__nonnull__(test);
	test->result = result;
}

void oval_result_test_set_instance(struct oval_result_test *test, int instance)
{
	__attribute__nonnull__(test);
	test->instance = instance;
}

void oval_result_test_add_message(struct oval_result_test *test, struct oval_message *message) 
{
	__attribute__nonnull__(test);
	oval_collection_add(test->messages, message);
}

void oval_result_test_add_item(struct oval_result_test *test, struct oval_result_item *item) 
{
	__attribute__nonnull__(test);
	oval_collection_add(test->items, item);
}

void oval_result_test_add_binding(struct oval_result_test *test, struct oval_variable_binding *binding) 
{
	__attribute__nonnull__(test);
	oval_collection_add(test->bindings, binding);
}

//void(*oscap_consumer_func)(void*, void*);
static void _oval_test_message_consumer(struct oval_message *message, struct oval_result_test *test) {
	oval_result_test_add_message(test, message);
}

static int _oval_result_test_binding_parse(xmlTextReaderPtr reader, struct oval_parser_context *context, void **args) {
	int return_code = 0;

	xmlChar *variable_id = xmlTextReaderGetAttribute(reader, BAD_CAST "variable_id");

	struct oval_syschar_model *syschar_model = oval_result_system_get_syschar_model(SYSTEM);
	struct oval_definition_model *definition_model = oval_syschar_model_get_definition_model(syschar_model);

	struct oval_variable *variable = oval_definition_model_get_new_variable
	    (definition_model, (char *)variable_id, OVAL_VARIABLE_UNKNOWN);

	xmlChar *value = xmlTextReaderValue(reader);

	struct oval_variable_binding *binding = oval_variable_binding_new(variable, oscap_strdup((char *) value));
	oval_result_test_add_binding(TEST, binding);

	xmlFree(value);
	xmlFree(variable_id);

	return return_code;
}

static int _oval_result_test_parse(xmlTextReaderPtr reader, struct oval_parser_context *context, void **args) {
	int return_code = 0;
	xmlChar *localName = xmlTextReaderLocalName(reader);

	if (strcmp((const char *)localName, "message") == 0) {
		return_code = oval_message_parse_tag(reader, context, (oscap_consumer_func) _oval_test_message_consumer, TEST);
	} else if (strcmp((const char *)localName, "tested_item") == 0) {
		return_code = oval_result_item_parse_tag(reader, context, SYSTEM, (oscap_consumer_func) _oval_test_item_consumer, args);
	} else if (strcmp((const char *)localName, "tested_variable") == 0) {
		return_code = _oval_result_test_binding_parse(reader, context, args);
	} else {
		dW( "Unhandled tag: <%s>.\n", localName);
		oval_parser_skip_tag(reader, context);
	}

	oscap_free(localName);

	return return_code;
}

int oval_result_test_parse_tag(xmlTextReaderPtr reader, struct oval_parser_context *context, void *usr) {

	struct oval_result_system *sys = (struct oval_result_system *) usr;
	int return_code = 0;
	struct oval_definition_model *dmod;
	struct oval_test *dtst;
	struct oval_result_test *test;
	xmlChar *test_id = xmlTextReaderGetAttribute(reader, BAD_CAST "test_id");

	dmod = context->definition_model;
	dtst = oval_definition_model_get_new_test(dmod, (char *) test_id);
	test = oval_result_system_get_new_test(sys, dtst);
	if (test == NULL)
		return -1;

	oval_result_t result = oval_result_parse(reader, "result", 0);
	oval_result_test_set_result(test, result);
	int variable_instance = oval_parser_int_attribute(reader, "variable_instance", 1);
	oval_result_test_set_instance(test, variable_instance);

	struct oval_test *ovaltst = oval_result_test_get_test(test);

	oval_existence_t check_existence = oval_existence_parse(reader, "check_existence", OVAL_AT_LEAST_ONE_EXISTS);
	oval_existence_t tst_check_existence = oval_test_get_existence(ovaltst);
	if (tst_check_existence == OVAL_EXISTENCE_UNKNOWN) {
		oval_test_set_existence(ovaltst, check_existence);
	} else if (tst_check_existence != check_existence) {
		oscap_dlprintf(DBG_W, "@check_existence does not match, test_id: %s.\n", test_id);
	}

	oval_check_t check = oval_check_parse(reader, "check", OVAL_CHECK_UNKNOWN);
	oval_check_t tst_check = oval_test_get_check(ovaltst);
	if (tst_check == OVAL_CHECK_UNKNOWN) {
		oval_test_set_check(ovaltst, check);
	} else if (tst_check != check) {
		oscap_dlprintf(DBG_W, "@check does not match, test_id: %s.\n", test_id);
	}

	int version = oval_parser_int_attribute(reader, "version", 0);
	int tst_version = oval_test_get_version(ovaltst);
	if (tst_version == 0) {
		oval_test_set_version(ovaltst, version);
	} else if (tst_version != version) {
		oscap_dlprintf(DBG_W, "@version does not match, test_id: %s.\n", test_id);
	}

	struct oval_string_map *itemmap = oval_string_map_new();
	void *args[] = { sys, test, itemmap };
	return_code = oval_parser_parse_tag(reader, context, (oval_xml_tag_parser) _oval_result_test_parse, args);
	oval_string_map_free(itemmap, NULL);
	test->bindings_initialized = true;
	test->bindings_clearable = true;

	oscap_free(test_id);
	return return_code;
}

static void _oval_result_binding_to_dom(struct oval_variable_binding *binding, xmlDocPtr doc, xmlNode *parent) {
	xmlNs *ns_results = xmlSearchNsByHref(doc, parent, OVAL_RESULTS_NAMESPACE);
	struct oval_variable *oval_variable = oval_variable_binding_get_variable(binding);
	char *variable_id = oval_variable_get_id(oval_variable);
	struct oval_string_iterator *str_itr;

	str_itr = oval_variable_binding_get_values(binding);
	while (oval_string_iterator_has_more(str_itr)) {
		char *value;
		xmlNode *binding_node;

		value = oval_string_iterator_next(str_itr);
		binding_node = xmlNewTextChild(parent, ns_results, BAD_CAST "tested_variable", BAD_CAST value);
		xmlNewProp(binding_node, BAD_CAST "variable_id", BAD_CAST variable_id);
	}
	oval_string_iterator_free(str_itr);
}

xmlNode *oval_result_test_to_dom(struct oval_result_test *rslt_test, xmlDocPtr doc, xmlNode * parent) {
	__attribute__nonnull__(rslt_test);

	xmlNs *ns_results = xmlSearchNsByHref(doc, parent, OVAL_RESULTS_NAMESPACE);
	xmlNode *test_node = xmlNewTextChild(parent, ns_results, BAD_CAST "test", NULL);

	struct oval_test *oval_test = oval_result_test_get_test(rslt_test);
	char *test_id = oval_test_get_id(oval_test);
	xmlNewProp(test_node, BAD_CAST "test_id", BAD_CAST test_id);

	char version[10];
	*version = '\0';
	snprintf(version, sizeof(version), "%d", oval_test_get_version(oval_test));
	xmlNewProp(test_node, BAD_CAST "version", BAD_CAST version);

	oval_existence_t existence = oval_test_get_existence(oval_test);
	if (existence != OVAL_AT_LEAST_ONE_EXISTS) {
		xmlNewProp(test_node, BAD_CAST "check_existence", BAD_CAST oval_existence_get_text(existence));
	}

	oval_check_t check = oval_test_get_check(oval_test);
	xmlNewProp(test_node, BAD_CAST "check", BAD_CAST oval_check_get_text(check));

	int instance_val = oval_result_test_get_instance(rslt_test);
	if (instance_val > 1) {
		char instance[10];
		*instance = '\0';
		snprintf(instance, sizeof(instance), "%d", instance_val);
		xmlNewProp(test_node, BAD_CAST "variable_instance", BAD_CAST instance);
	}

	oval_result_t result = oval_result_test_get_result(rslt_test);
	xmlNewProp(test_node, BAD_CAST "result", BAD_CAST oval_result_get_text(result));

	/* does not make sense to report these when test(definition) is not evaluated */
	if( result != OVAL_RESULT_NOT_EVALUATED) {
		struct oval_result_item_iterator *items = oval_result_test_get_items(rslt_test);
		while (oval_result_item_iterator_has_more(items)) {
			struct oval_result_item *item = oval_result_item_iterator_next(items);
			oval_result_item_to_dom(item, doc, test_node);
		}
		oval_result_item_iterator_free(items);

		struct oval_variable_binding_iterator *bindings = oval_result_test_get_bindings(rslt_test);
		while (oval_variable_binding_iterator_has_more(bindings)) {
			struct oval_variable_binding *binding = oval_variable_binding_iterator_next(bindings);
			_oval_result_binding_to_dom(binding, doc, test_node);
		}
		oval_variable_binding_iterator_free(bindings);
	}

	return test_node;
}
