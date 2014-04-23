/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

int keyset_new();
void keyset_delete(int ks);
int keyset_is_valid(int ks);
int keyset_keyid(const char *key, int create);
int keyset_is_valid_keyid(int kid);
const char *keyset_key(int kid);
void keyset_set(int ks, int kid, char value);
char keyset_get(int ks, int kid);
void keyset_for_all_key(void (*cb)(const char*,int,void*), void *extra);
void keyset_for_all_keyset(void (*cb)(int,void*), void *extra);
void keyset_for_all(int ks, void (*cb)(const char*,int,char,void*), void *extra);
void keyset_for_all_not_null(int ks, void (*cb)(const char*,int,char,void*), void *extra);
