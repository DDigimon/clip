/**
 * @file elements.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "public/oscap.h"
#include "util.h"
#include "list.h"
#include "elements.h"


const struct oscap_string_map OSCAP_BOOL_MAP[] = {
	{true, "true"}, {true, "True"}, {true, "TRUE"},
	{true, "yes"}, {true, "Yes"}, {true, "YES"},
	{true, "1"}, {false, NULL}
};


bool oscap_to_start_element(xmlTextReaderPtr reader, int depth)
{
	//int olddepth = xmlTextReaderDepth(reader);
	while (xmlTextReaderDepth(reader) >= depth) {
		switch (xmlTextReaderNodeType(reader)) {
			//TODO: change int values to macros XML_ELEMENT_TYPE_*
		case XML_READER_TYPE_ELEMENT:
			if (xmlTextReaderDepth(reader) == depth)
				return true;
		default:
			break;
		}
		if (xmlTextReaderRead(reader) != 1)
			break;
	}
	return false;
}

char *oscap_element_string_copy(xmlTextReaderPtr reader)
{
	int t;

	if (xmlTextReaderIsEmptyElement(reader))
		return NULL;

	t = xmlTextReaderNodeType(reader);
	if (t == XML_ELEMENT_NODE || t == XML_ATTRIBUTE_NODE)
		xmlTextReaderRead(reader);
	if (xmlTextReaderHasValue(reader))
		return (char *)xmlTextReaderValue(reader);
	else
		return (char *) calloc(1,1);
}

const char *oscap_element_string_get(xmlTextReaderPtr reader)
{
	if (xmlTextReaderNodeType(reader) == 1 || xmlTextReaderNodeType(reader) == 2)
		xmlTextReaderRead(reader);
	if (xmlTextReaderHasValue(reader))
		return (const char *)xmlTextReaderConstValue(reader);
	return NULL;
}

int oscap_element_depth(xmlTextReaderPtr reader)
{
	int depth = xmlTextReaderDepth(reader);
	switch (xmlTextReaderNodeType(reader)) {
	case 2:
	case 5:
	case 3:
		return depth - 1;
	default:
		return depth;
	}
}

char *oscap_get_xml(xmlTextReaderPtr reader)
{
	return (char *)xmlTextReaderReadInnerXml(reader);
}

time_t oscap_get_date(const char *date)
{
	if (date) {
		struct tm tm;
		memset(&tm, 0, sizeof(tm));
		if (sscanf(date, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
			tm.tm_mon -= 1;
			tm.tm_year -= 1900;
			return mktime(&tm);
		}
	}
	return 0;
}

time_t oscap_get_datetime(const char *date)
{
	if (date) {
		struct tm tm;
		memset(&tm, 0, sizeof(tm));
		if (sscanf
		    (date, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
		     &tm.tm_sec) == 6) {
			tm.tm_mon -= 1;
			tm.tm_year -= 1900;
			return mktime(&tm);
		}
	}
	return 0;
}

xmlNode *oscap_xmlstr_to_dom(xmlNode *parent, const char *elname, const char *content)
{
	char *str = oscap_sprintf("<x xmlns:xhtml='http://www.w3.org/1999/xhtml'>%s</x>", content);
	xmlDoc *doc = xmlReadMemory(str, strlen(str), NULL, NULL,
		XML_PARSE_RECOVER | XML_PARSE_NOERROR | XML_PARSE_NOWARNING | XML_PARSE_NONET | XML_PARSE_NSCLEAN);
	xmlNode *text_node = xmlCopyNode(xmlDocGetRootElement(doc), 1);
	xmlNodeSetName(text_node, BAD_CAST elname);
	xmlAddChild(parent, text_node);
	xmlFreeDoc(doc);
	oscap_free(str);
	return text_node;
}


const char *oscap_strlist_find_value(char ** const kvalues, const char *key)
{
	if (kvalues == NULL || key == NULL) return NULL;

	for (int i = 0; kvalues[i] != NULL && kvalues[i + 1] != NULL; i += 2)
		if (strcmp(kvalues[i], key) == 0)
			return kvalues[i + 1];

	return NULL;
}


