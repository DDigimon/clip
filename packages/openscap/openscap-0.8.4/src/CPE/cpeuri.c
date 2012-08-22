/*! \file cpeuri.c
 *  \brief Interface to Common Platform Enumeration (CPE) URI
 *  
 *   See more details at http://nvd.nist.gov/cpe.cfm
 *  
 */

/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
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
 *      Brandon Dixon  <brandon.dixon@g2-inc.com>
 *      Lukas Kuklinek <lkuklinek@redhat.com>
 */

#define _BSD_SOURCE

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <pcre.h>
#include <ctype.h>

#include "cpeuri.h"
#include "common/util.h"

#define CPE_URI_SUPPORTED "2.2"

// enumeration of CPE URI fields (useful for indexing arrays)
enum cpe_field_t {
	CPE_FIELD_TYPE,
	CPE_FIELD_VENDOR,
	CPE_FIELD_PRODUCT,
	CPE_FIELD_VERSION,
	CPE_FIELD_UPDATE,
	CPE_FIELD_EDITION,
	CPE_FIELD_LANGUAGE,
	CPE_FIELDNUM,
};

struct cpe_name {
//      char *uri;       // complete URI cache
	cpe_part_t part;	// part
	char *vendor;		// vendor
	char *product;		// product
	char *version;		// version
	char *update;		// update
	char *edition;		// edition
	char *language;		// language
};

/* h - hardware
 * o - OS
 * a - application
 */
//static const char *CPE_PART_CHAR[] = { NULL, "h", "o", "a" };

static const char CPE_SEP_CHAR = ':';

static const struct oscap_string_map CPE_PART_MAP[] = {
	{ CPE_PART_HW,   "h"  },
	{ CPE_PART_OS,   "o"  },
	{ CPE_PART_APP,  "a"  },
	{ CPE_PART_NONE, NULL }
};

char **cpe_uri_split(char *str, const char *delim);
static bool cpe_urldecode(char *str);
bool cpe_name_check(const char *str);
static const char *cpe_get_field(const struct cpe_name *cpe, int idx);
static const char *as_str(const char *str);
static bool cpe_set_field(struct cpe_name *cpe, int idx, const char *newval);
/*
 * Fill @a cpe structure with parsed @a fields.
 *
 * Fields can be obtained via cpe_split().
 * Pointers in target sructure will point to same strings as pointers in @a fields do.
 * No string duplication is performed.
 *
 * @see cpe_split
 * @param cpe structure to be filled
 * @param fields NULL-terminated array of strings representing individual fields
 * @return true on success
 */
static bool cpe_assign_values(struct cpe_name *cpe, char **fields);

static int cpe_fields_num(const struct cpe_name *cpe)
{
	__attribute__nonnull__(cpe);

	if (cpe == NULL)
		return 0;
	int maxnum = 0;
	int i;
	for (i = 0; i < CPE_FIELDNUM; ++i)
		if (cpe_get_field(cpe, i))
			maxnum = i + 1;
	return maxnum;
}

static const char *cpe_get_field(const struct cpe_name *cpe, int idx)
{
	__attribute__nonnull__(cpe);

	if (cpe == NULL)
		return NULL;

	switch (idx) {
	case 0:
		return oscap_enum_to_string(CPE_PART_MAP, cpe->part);
	case 1:
		return cpe->vendor;
	case 2:
		return cpe->product;
	case 3:
		return cpe->version;
	case 4:
		return cpe->update;
	case 5:
		return cpe->edition;
	case 6:
		return cpe->language;
	default:
		assert(false);
		return NULL;
	}
}

bool cpe_set_field(struct cpe_name * cpe, int idx, const char *newval)
{

	__attribute__nonnull__(cpe);
	/*__attribute__nonnull__(newval); <-- can't be here, we want to set NULL */

	if (cpe == NULL)
		return false;

	char **fieldptr = NULL;
	switch (idx) {
	case 0:
		cpe->part = oscap_string_to_enum(CPE_PART_MAP, newval);
		return cpe->part != CPE_PART_NONE;
	case 1:
		fieldptr = &cpe->vendor;
		break;
	case 2:
		fieldptr = &cpe->product;
		break;
	case 3:
		fieldptr = &cpe->version;
		break;
	case 4:
		fieldptr = &cpe->update;
		break;
	case 5:
		fieldptr = &cpe->edition;
		break;
	case 6:
		fieldptr = &cpe->language;
		break;
	default:
		assert(false);
		return false;
	}

	oscap_free(*fieldptr);
	if (newval && strcmp(newval, "") == 0)
		newval = NULL;
	if (newval != NULL)
		*fieldptr = strdup(newval);
	else
		*fieldptr = NULL;

	return true;
}

struct cpe_name *cpe_name_new(const char *cpestr)
{

	int i;
	struct cpe_name *cpe;

	if (cpestr && !cpe_name_check(cpestr))
		return NULL;

	cpe = oscap_alloc(sizeof(struct cpe_name));
	if (cpe == NULL)
		return NULL;
	memset(cpe, 0, sizeof(struct cpe_name));	// zero memory

	if (cpestr) {
		char *data_ = strdup(cpestr + 5);	// without 'cpe:/'
		char **fields_ = oscap_split(data_, ":");
		for (i = 0; fields_[i]; ++i)
			cpe_urldecode(fields_[i]);
		cpe_assign_values(cpe, fields_);
		oscap_free(data_);
		oscap_free(fields_);
	}
	return cpe;
}

struct cpe_name * cpe_name_clone(struct cpe_name * old_name)
{
        struct cpe_name * new_name = oscap_alloc(sizeof(struct cpe_name));
        if (new_name == NULL) 
            return NULL;

	new_name->part = old_name->part;
	new_name->vendor = oscap_strdup(old_name->vendor);
	new_name->product = oscap_strdup(old_name->product);
	new_name->version = oscap_strdup(old_name->version);
	new_name->update = oscap_strdup(old_name->update);
	new_name->edition = oscap_strdup(old_name->edition);
	new_name->language = oscap_strdup(old_name->language);

        return new_name;
}


static bool cpe_urldecode(char *str)
{

	__attribute__nonnull__(str);

	char *inptr, *outptr;

	for (inptr = outptr = str; *inptr; ++inptr) {
		if (*inptr == '%') {
			if (isxdigit(inptr[1]) && isxdigit(inptr[2])) {
				char hex[3] = { inptr[1], inptr[2], '\0' };
				unsigned out;
				sscanf(hex, "%x", &out);
				// if (out == 0) return false; // this is strange
				*outptr++ = out;
				inptr += 2;
			} else {
				*outptr = '\0';
				return false;
			}
		} else
			*outptr++ = *inptr;
	}
	*outptr = '\0';
	return true;
}

bool cpe_name_match_one(const struct cpe_name * cpe, const struct cpe_name * against)
{

	int i;
	if (cpe == NULL || against == NULL)
		return false;

	int cpefn = cpe_fields_num(cpe);
	if (cpe_fields_num(against) < cpefn)
		return false;

	for (i = 0; i < cpefn; ++i) {
		const char *cpefield = cpe_get_field(cpe, i);
		if (cpefield && strcasecmp(cpefield, as_str(cpe_get_field(against, i))) != 0)
			return false;
	}

	return true;
}

bool cpe_name_match_cpes(const struct cpe_name * name, size_t n, struct cpe_name ** namelist)
{

	__attribute__nonnull__(name);
	__attribute__nonnull__(namelist);

	if (name == NULL || namelist == NULL)
		return false;

	for (size_t i = 0; i < n; ++i)
		if (cpe_name_match_one(name, namelist[i]))
			return true;
	return false;
}

int cpe_name_match_strs(const char *candidate, size_t n, char **targets)
{
	__attribute__nonnull__(candidate);
	__attribute__nonnull__(targets);

	int i;
	struct cpe_name *ccpe, *tcpe;

	ccpe = cpe_name_new(candidate);	// candidate cpe
	if (ccpe == NULL)
		return -2;

	for (i = 0; i < (int)n; ++i) {
		tcpe = cpe_name_new(targets[i]);	// target cpe

		if (cpe_name_match_one(ccpe, tcpe)) {
			// CPE matched
			cpe_name_free(ccpe);
			cpe_name_free(tcpe);
			return i;
		}

		cpe_name_free(tcpe);
	}

	cpe_name_free(ccpe);
	return -1;
}

bool cpe_name_check(const char *str)
{
	__attribute__nonnull__(str);

	if (str == NULL)
		return false;

	pcre *re;
	const char *error;
	int erroffset;
	re = pcre_compile("^cpe:/[aho]?(:[a-z0-9._~%-]*){0,6}$", PCRE_CASELESS, &error, &erroffset, NULL);

	int rc;
	int ovector[30];
	rc = pcre_exec(re, NULL, str, strlen(str), 0, 0, ovector, 30);

	pcre_free(re);

	return rc >= 0;
}

static const char *as_str(const char *str)
{
	if (str == NULL)
		return "";
	return str;
}

static char *cpe_urlencode(const char *str)
{
	//return strdup(str);
	if (str == NULL)
		return NULL;

	// allocate enough space
	char *result = oscap_alloc(strlen(str) * 3 * sizeof(char) + 1);
	char *out = result;

	for (const char *in = str; *in != '\0'; ++in, ++out) {
		if (isalnum(*in) || strchr("-._~", *in))
			*out = *in;
		else {
			// this char shall be %-encoded
			sprintf(out, "%%%02X", *in);
			out += 2;
		}
	}

	*out = '\0';

	return result;
}

char *cpe_name_get_uri(const struct cpe_name *cpe)
{
	__attribute__nonnull__(cpe);

	int len = 16;
	int i;
	char *result;
	char* part[CPE_FIELDNUM] = { NULL }; // CPE URI parts

	if (cpe == NULL)
		return NULL;

	// get individual parts (%-encded)
	for (i = 0; i < CPE_FIELDNUM; ++i) {
		part[i] = cpe_urlencode(as_str(cpe_get_field(cpe, i)));
		len += strlen(part[i]);
	}

	result = oscap_alloc(len * sizeof(char));
	if (result == NULL)
		return NULL;

	// create the URI
	i = snprintf(result, len, "cpe:/%s:%s:%s:%s:%s:%s:%s",
		part[0], part[1], part[2], part[3], part[4], part[5], part[6]
	);

	// free individual parts
	for (int j = 0; j < CPE_FIELDNUM; ++j)
		oscap_free(part[j]);

	// trim trailing colons
	while (result[--i] == ':')
		result[i] = '\0';

	return result;
}

int cpe_name_write(const struct cpe_name *cpe, FILE * f)
{
	__attribute__nonnull__(cpe);
	__attribute__nonnull__(f);

	int ret;
	char *uri;

	uri = cpe_name_get_uri(cpe);
	if (uri == NULL)
		return EOF;

	ret = fprintf(f, "%s", uri);

	oscap_free(uri);
	return ret;
}

static bool cpe_assign_values(struct cpe_name *cpe, char **fields)
{
	__attribute__nonnull__(cpe);
	__attribute__nonnull__(fields);

	int i;

	if (cpe == NULL || fields == NULL)
		return false;

	for (i = 0; fields[i]; ++i)
		cpe_set_field(cpe, i, fields[i]);

	return true;
}

void cpe_name_free(struct cpe_name *cpe)
{

	if (cpe == NULL)
		return;

	int i;
	for (i = 0; i < CPE_FIELDNUM; ++i)
		cpe_set_field(cpe, i, NULL);
	oscap_free(cpe);
}

const char * cpe_name_supported(void)
{
        return CPE_URI_SUPPORTED;
}

OSCAP_ACCESSOR_SIMPLE(cpe_part_t, cpe_name, part)
    OSCAP_ACCESSOR_STRING(cpe_name, vendor)
    OSCAP_ACCESSOR_STRING(cpe_name, product)
    OSCAP_ACCESSOR_STRING(cpe_name, version)
    OSCAP_ACCESSOR_STRING(cpe_name, update)
    OSCAP_ACCESSOR_STRING(cpe_name, edition)
    OSCAP_ACCESSOR_STRING(cpe_name, language)
