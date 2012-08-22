/**
 * @addtogroup CPE
 * @{
 * @addtogroup cpeuri CPE URIs
 * @{
 *
 *
 * @file cpeuri.h
 * \brief Interface to Common Platform Enumeration (CPE) URI
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
 *      Lukas Kuklinek <lkuklinek@redhat.com>
 */

#ifndef _CPEURI_H_
#define _CPEURI_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/// enumeration of possible CPE parts
typedef enum {
	CPE_PART_NONE,		///< no part specified
	CPE_PART_HW,		///< hardware
	CPE_PART_OS,		///< operating system
	CPE_PART_APP		///< application
} cpe_part_t;

/**
 * @struct cpe_name
 * Structure holding Common Platform Enumeration URI data.
 *
 * Empty components are set to NULL.
 */
struct cpe_name;

/**
 * Create a new CPE structure from string @a cpe.
 *
 * @memberof cpe_name
 * @note If @a cpe is NULL, empty cpe will be created.
 * @param cpe CPE URI string to be parsed
 * @return new structure holding parsed data
 * @retval NULL on failure
 */
struct cpe_name *cpe_name_new(const char *cpe);

/**
 * Clone CPE Name
 * @param old_name CPE name
 * @memberof cpe_name
 */
struct cpe_name * cpe_name_clone(struct cpe_name * old_name);

/**
 * Destructor. Frees any used resources and safely destroys @a cpe.
 * @memberof cpe_name
 * @param cpe CPE to be deleted
 */
void cpe_name_free(struct cpe_name *cpe);

/************************************************************/
/**
 * @name Getters
 * Return value is pointer to structure's member. Do not free unless you null the pointer in the structure. 
 * Use remove function otherwise.
 * @{
 * */

/**
 * Get CPE name part type field.
 * @memberof cpe_name
 */
cpe_part_t cpe_name_get_part(const struct cpe_name *cpe);

/**
 * Get CPE name vendor field.
 * @memberof cpe_name
 */
const char *cpe_name_get_vendor(const struct cpe_name *cpe);

/**
 * Get CPE name product field.
 * @memberof cpe_name
 */
const char *cpe_name_get_product(const struct cpe_name *cpe);

/**
 * Get CPE name version field.
 * @memberof cpe_name
 */
const char *cpe_name_get_version(const struct cpe_name *cpe);

/**
 * Get CPE name update field.
 * @memberof cpe_name
 */
const char *cpe_name_get_update(const struct cpe_name *cpe);

/**
 * Get CPE name edition field.
 * @memberof cpe_name
 */
const char *cpe_name_get_edition(const struct cpe_name *cpe);

/**
 * Get CPE name language field.
 * @memberof cpe_name
 */
const char *cpe_name_get_language(const struct cpe_name *cpe);

/**
 * Return CPE URI as a new string.
 * @memberof cpe_name
 * @note Returned string is newly allocated and is caller's responsibility to free it.
 * @param cpe CPE to be converted
 * @return CPE URI as string
 * @retval NULL on failure
 */
char *cpe_name_get_uri(const struct cpe_name *cpe);

/************************************************************/
/** @} End of Getters group */

/************************************************************/
/**
 * @name Setters
 * For lists use add functions. Parameters of set functions are duplicated in memory and need to 
 * be freed by caller.
 * @{
 */

/**
 * Set CPE name part type field.
 * @memberof cpe_name
 */
bool cpe_name_set_part(struct cpe_name *cpe, cpe_part_t newval);

/**
 * Set CPE name vendor field.
 * @memberof cpe_name
 */
bool cpe_name_set_vendor(struct cpe_name *cpe, const char *newval);

/**
 * Set CPE name product field.
 * @memberof cpe_name
 */
bool cpe_name_set_product(struct cpe_name *cpe, const char *newval);

/**
 * Set CPE name version field.
 * @memberof cpe_name
 */
bool cpe_name_set_version(struct cpe_name *cpe, const char *newval);

/**
 * Set CPE name update field.
 * @memberof cpe_name
 */
bool cpe_name_set_update(struct cpe_name *cpe, const char *newval);

/**
 * Set CPE name edition field.
 * @memberof cpe_name
 */
bool cpe_name_set_edition(struct cpe_name *cpe, const char *newval);

/**
 * Set CPE name language field.
 * @memberof cpe_name
 */
bool cpe_name_set_language(struct cpe_name *cpe, const char *newval);

/************************************************************/
/** @} End of Setters group */

/************************************************************/
/**
 * @name Evaluators
 * @{
 * */

/**
 * Check if candidate CPE @a cpe matches CPE @a against
 * according to CPE specification v 2.1.
 * @memberof cpe_name
 */
bool cpe_name_match_one(const struct cpe_name *cpe, const struct cpe_name *against);

/**
 * Check if CPE @a name matches any CPE in @a namelist.
 * @memberof cpe_name
 * @param name name to be looked-up
 * @param n number of items in namelist
 * @param namelist list of names to search in
 * @return true if @a name was found within @a namelist
 */
bool cpe_name_match_cpes(const struct cpe_name *name, size_t n, struct cpe_name **namelist);

/**
 * Write CPE URI @a cpe to file a descriptor @a f
 * @memberof cpe_name
 * @param cpe cpe to write
 * @param f file descriptor to write CPE URI to
 * @return number of written characters
 * @retval <0 on failure
 */
int cpe_name_write(const struct cpe_name *cpe, FILE * f);

/**
 * Ensures @a str is in proper CPE format.
 * @memberof cpe_name
 * @param str string to be validated
 */
bool cpe_name_check(const char *str);

/**
 * Match CPE URI @a candidate against list of @a n CPE URIs given by @a targets.
 * @memberof cpe_name
 * @param candidate candidarte CPE URI as string
 * @param n number of items in targets
 * @param targets list of CPE URIs to be candidate matched against
 * @return index of first URI in targets, that matched
 * @retval -1 on mismatch
 * @retval -2 invalid CPE URI was given as parameter
 */
int cpe_name_match_strs(const char *candidate, size_t n, char **targets);

/**
 * Get supported version of CPE uri XML
 * @return version of XML file format
 * @memberof cpe_name
 */
const char * cpe_name_supported(void);

/************************************************************/
/** @} End of Evaluators group */

/*@}*/

/*@}*/

#endif				/* _CPEURI_H_ */
