/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#ifndef KEYZEN_H
#define KEYZEN_H

#include "keyzen-constants.h"

/*
 * Tests if the process of 'pid' is granted for all the 'count' 'keys'.
 *
 * Return 0 if the all keys are granted or a negative code on failure.
 */ 
int keyzen_process_has_keys(pid_t pid, const char **keys, int count);

/*
 * Gets the 'list' of keys possible for the process of 'pid'.
 *
 * Returns 0 on sucess or a negative code on failure.
 *
 * On success, 'list' will be filled on success with an 
 * opaque value that the client should free either with 
 * `free` or `keyzen_list_keys_free`.
 *
 * The returned 'list' is used through the functions
 * `keyzen_list_keys_count`, `keyzen_list_keys_name`
 * and `keyzen_list_keys_free` (aka `free`).
 */
int keyzen_process_list_keys(pid_t pid, void **list);

/*
 * Tests if the 'key' is authorized for the process of 'pid'.
 *
 * Return 0 if the key is granted or a negative code on failure.
 */
int keyzen_process_has_key(pid_t pid, const char *key);

/*
 * Tests if the current process is granted for all the 'count' 'keys'.
 *
 * Return 0 if the all keys are granted or a negative code on failure.
 */
int keyzen_self_has_keys(const char **keys, int count);

/*
 * Adds the 'count' 'keys' to the current process.
 *
 * Return 0 on success or a negative code on failure.
 */
int keyzen_self_add_keys(const char **keys, int count);

/*
 * Drops the 'count' 'keys' of the current process.
 *
 * Return 0 on success or a negative code on failure.
 */
int keyzen_self_drop_keys(const char **keys, int count);

/*
 * Sets the 'count' 'keys' for the current process,
 * adding or dropping the keys on need to have at end
 * the given set.
 *
 * Return 0 on success or a negative code on failure.
 */
int keyzen_self_set_keys(const char **keys, int count);

/*
 * Tests if the 'key' is authorized for the current process.
 *
 * Return 0 if the key is granted or a negative code on failure.
 */
int keyzen_self_has_key(const char *key);

/*
 * Adds the 'key' to the current process.
 *
 * Return 0 on success or a negative code on failure.
 */
int keyzen_self_add_key(const char *key);

/*
 * Drops the 'key' to the current process.
 *
 * Return 0 on success or a negative code on failure.
 */
int keyzen_self_drop_key(const char *key);

/*
 * Tests if the current process can admin its keys.
 *
 * Return 0 if can admin or a negative code on failure.
 */
int keyzen_is_self_admin();

/*
 * Gets the 'list' of keys possible for the current process.
 *
 * Returns 0 on sucess or a negative code on failure.
 *
 * On success, 'list' will be filled on success with an 
 * opaque value that the client should free either with 
 * `free` or `keyzen_list_keys_free`.
 *
 * The returned 'list' is used through the functions
 * `keyzen_list_keys_count`, `keyzen_list_keys_name`
 * and `keyzen_list_keys_free` (aka `free`).
 */
int keyzen_self_list_keys(void **list);

/*
 * Returns the count of keys in the 'list'.
 */
int keyzen_list_keys_count(void *list);

/*
 * Returns the key name of 'index' in 'list' or 0
 * if 'index' is invalid. The valid indexes are
 * from 1 to the count returned by `keyzen_list_keys_count`.
 */
const char *keyzen_list_keys_name(void *list, int index);

/*
 * free the 'list'
 */
#define keyzen_list_keys_free(list) free(list)

#endif

