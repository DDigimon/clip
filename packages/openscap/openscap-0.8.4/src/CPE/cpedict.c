/**
 * @file cpedict.c
 * \brief Interface to Common Platform Enumeration (CPE) Dictionary.
 *
 * See more details at http://nvd.nist.gov/cpe.cfm
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
 *      Maros Barabas <mbarabas@redhat.com>
 *      Lukas Kuklinek <lkuklinek@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "public/cpedict.h"
#include "cpedict_priv.h"

#include "common/list.h"
#include "common/util.h"

#define CPE_DICT_SUPPORTED "2.2"

struct cpe_dict_model *cpe_dict_model_import(const char *file)
{

	__attribute__nonnull__(file);

	if (file == NULL)
		return NULL;

	struct cpe_dict_model *dict;

	dict = cpe_dict_model_parse_xml(file);

	return dict;
}

void cpe_dict_model_export(const struct cpe_dict_model *dict, const char *file)
{

	__attribute__nonnull__(dict);
	__attribute__nonnull__(file);

	if (file == NULL)
		return;

	cpe_dict_model_export_xml(dict, file);

}

bool cpe_name_match_dict(struct cpe_name * cpe, struct cpe_dict_model * dict)
{

	__attribute__nonnull__(cpe);
	__attribute__nonnull__(dict);

	if (cpe == NULL || dict == NULL)
		return false;

	struct cpe_item_iterator *items = cpe_dict_model_get_items(dict);
	size_t n = oscap_iterator_get_itemcount((struct oscap_iterator *)items);
	struct cpe_name **cpes = oscap_alloc(sizeof(struct cpe_name *) * n);
	//struct oscap_list_item* cur = ((struct oscap_list *) cpe_dict_model_get_items(dict))->first;

	int i = 0;
	OSCAP_FOREACH(cpe_item, item, items, cpes[i++] = cpe_item_get_name(item);)

	bool ret = cpe_name_match_cpes(cpe, n, cpes);

	oscap_free(cpes);

	return ret;
}

bool cpe_name_match_dict_str(const char *cpestr, struct cpe_dict_model * dict)
{
	__attribute__nonnull__(cpestr);
	__attribute__nonnull__(dict);

	bool ret;
	if (cpestr == NULL)
		return false;
	struct cpe_name *cpe = cpe_name_new(cpestr);
	if (cpe == NULL)
		return false;
	ret = cpe_name_match_dict(cpe, dict);
	cpe_name_free(cpe);
	return ret;
}

const char * cpe_dict_model_supported(void) 
{
        return CPE_DICT_SUPPORTED;
}

