/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "keyset.h"

static int tag_count = 0;
static char **tag_strings = 0;
static int *tag_indexes = 0;

static int keyset_count = 0;
static int keyset_size = 1;
static int *keysets = 0;
static int keyset_free = -1;

#define ispow2(x)   (!((x) & ((x) - 1)))

#define ALLOC_KEYSET_COUNT	16 /* count of keyset to reserve by alloc */
#define ALLOC_TAG_COUNT		16 /* count of tag to reserve by alloc, MUST be a power of 2 AND greater or equal to sizeof(int) */

#define sneed(x)	(assert(ispow2(ALLOC_TAG_COUNT)), (((x) + (ALLOC_TAG_COUNT-1)) & ~(ALLOC_TAG_COUNT-1)))

#define adrks(ks)  ((char*)(keysets + ((ks) * keyset_size)))

static int extend_keyset_size(int tagcnt)
{
	int size, i, j;
	int *p, *r;

	assert((tagcnt % sizeof * keysets) == 0);

	size = (tagcnt * sizeof(char) + (sizeof * keysets - 1)) / sizeof * keysets;

	if (keyset_count && size > keyset_size) {
		p = realloc(keysets, keyset_count * size * sizeof * keysets);
		if (!p)
			return -ENOMEM;

		keysets = p;
		r = p + keyset_count * keyset_size;
		p = p + keyset_count * size;
		i = keyset_count;
		while (i) {
			i--;
			p -= size;
			r -= keyset_size;
			j = size;
			while (keyset_size < j)
				p[--j] = 0;
			while (j) {
				j--;
				p[j] = r[j];
			}
		}
	}
	keyset_size = size;
	return 0;
}

static int reserve_tag_count(int tagcnt)
{
	int ns, s;
	void *p;

	/* grows on need */
	ns = sneed(tagcnt);
	s = sneed(tag_count);
	if (ns > s) {
		s = extend_keyset_size(ns);
		if (s)
			return s;
		p = realloc(tag_strings, ns * sizeof * tag_strings);
		if (!p)
			return -ENOMEM;
		tag_strings = p;
		p = realloc(tag_indexes, ns * sizeof * tag_indexes);
		if (!p)
			return -ENOMEM;
		tag_indexes = p;
	}
	return 0;
}

static int get_tag(const char *tag, int create)
{
	int i, k, l, u, d;
	char *s;

	assert(tag);

	/* dichotomic search */
	l = 0;
	u = tag_count;
	while (l < u)
	{
		i = (l + u) >> 1;
		k = tag_indexes[i];
		d = strcmp(tag, tag_strings[k]);
		if (!d)
			return k; /* found */
		if (d < 0)
			u = i;
		else
			l = i + 1;
	}

	/* not found */
	if (!create)
		return -ENOENT;

	/* reserve memory */
	l = reserve_tag_count(tag_count + 1);
	if (l)
		return l;

	/* allocation */
	s = strdup(tag);
	if (!s)
		return -ENOMEM;

	/* insertion now */
	k = tag_count++;
	tag_strings[k] = s;
	for (i = k ; i >= u ; i--)
		tag_indexes[i + 1] = tag_indexes[i];
	tag_indexes[u] = k;
	return k;
}

static int is_keyset_free(int ks)
{
	int iter;

	assert(0 <= ks && ks < keyset_count);
	iter = keyset_free;
	while (iter >= 0)
		if (iter == ks)
			return 1;
		else
			iter = keysets[iter * keyset_size];
	return 0;
}


int keyset_is_valid(int ks)
{
	return 0 <= ks && ks < keyset_count && !is_keyset_free(ks);
}

int keyset_keyid(const char *key, int create)
{
	assert(key);

	return get_tag(key, create);
}

int keyset_is_valid_keyid(int kid)
{
	return 0 <= kid && kid < tag_count;
}

const char *keyset_key(int kid)
{
	assert(keyset_is_valid_keyid(kid));

	return tag_strings[kid];
}




void keyset_set(int ks, int kid, char value)
{
	assert(keyset_is_valid(ks));
	assert(keyset_is_valid_keyid(kid));

	adrks(ks)[kid] = value;
}

char keyset_get(int ks, int kid)
{
	assert(keyset_is_valid(ks));
	assert(keyset_is_valid_keyid(kid));

	return adrks(ks)[kid];
}





int keyset_new()
{
	int r;
	int n;
	void *p;

	if (keyset_free < 0) {
		n = keyset_count + ALLOC_KEYSET_COUNT;
		p = realloc(keysets, n * keyset_size * sizeof * keysets);
		if (!p)
			return -ENOMEM;
		keysets = p;
		while (keyset_count < n) {
			keysets[keyset_count * keyset_size] = keyset_free;
			keyset_free = keyset_count++;
		}
	}
	r = keyset_free;
	keyset_free = keysets[r * keyset_size];
	memset(adrks(r), 0, keyset_size * sizeof * keysets);
	return r;
}

void keyset_delete(int ks)
{
	assert(keyset_is_valid(ks));

	keysets[ks * keyset_size] = keyset_free;
	keyset_free = ks;
}

void keyset_for_all_key(void (*cb)(const char*,int,void*), void *extra)
{
	int kid;

	for (kid = 0 ; kid < tag_count ; kid++)
		cb(tag_strings[kid], kid, extra);
}

void keyset_for_all_keyset(void (*cb)(int,void*), void *extra)
{
	int ks;

	for (ks = 0 ; ks < keyset_count ; ks ++)
		if (!is_keyset_free(ks))
			cb(ks, extra);
}

void keyset_for_all(int ks, void (*cb)(const char*,int,char,void*), void *extra)
{
	int kid;

	for (kid = 0 ; kid < tag_count ; kid++)
		cb(tag_strings[kid], kid, keyset_get(ks, kid), extra);
}

void keyset_for_all_not_null(int ks, void (*cb)(const char*,int,char,void*), void *extra)
{
	int kid;
	char value;

	for (kid = 0 ; kid < tag_count ; kid++) {
		value = keyset_get(ks, kid);
		if (value)
			cb(tag_strings[kid], kid, value, extra);
	}
}

