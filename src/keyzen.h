/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#ifndef KEYZEN_H
#define KEYZEN_H

#include "keyzen-constants.h"

int keyzen_process_has_keys(pid_t pid, const char **keys, int count);
int keyzen_process_list_keys(pid_t pid, void **list);
int keyzen_process_has_key(pid_t pid, const char *key);
int keyzen_self_has_keys(const char **keys, int count);
int keyzen_self_add_keys(const char **keys, int count);
int keyzen_self_drop_keys(const char **keys, int count);
int keyzen_self_set_keys(const char **keys, int count);
int keyzen_self_has_key(const char *key);
int keyzen_self_add_key(const char *key);
int keyzen_self_drop_key(const char *key);
int keyzen_is_self_admin();
int keyzen_self_list_keys(void **list);
int keyzen_list_keys_count(void *list);
char *keyzen_list_keys_name(void *list, int index);
#define keyzen_list_keys_free(x) free(x)

#endif

