#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

	
static int tag_count = 0;
static char **tag_strings = 0;
static int *tag_indexes = 0;

static int keyset_count = 0;
static int keyset_size = 0;
static unsigned *keysets = 0;

static unsigned *keyuses = 0;
static int keyset_iter = 0;
static int keyset_free = 0;

#define BITU      (CHAR_BIT * sizeof(unsigned))
#define sneed(x)  (((x) + (BITU-1)) & ~(BITU-1))

static int get_tag(const char *tag, int create)
{
	int i, k, l, u, d;
	void *p;
	char *s;

	/* search */
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

	/* grows on need */
	d = sneed(tag_count + 1);
	if (d != sneed(tag_count)) {
		p = realloc(tag_strings, d * sizeof * tag_strings);
		if (!p)
			return -ENOMEM;
		tag_strings = p;
		p = realloc(tag_indexes, d * sizeof * tag_indexes);
		if (!p)
			return -ENOMEM;
		tag_indexes = p;
		d /= BITU;
		p = realloc(keysets, keyset_count * d * sizeof * keysets);
		if (!p)
			return -ENOMEM;
		keysets = p;
		i = keyset_count;
		while (i) {
			i--;
			l = d;
			while (keyset_size < l) {
				l--;
				keysets[i * d + l] = 0;
			}
			while (l) {
				l--;
				keysets[i * d + l] = keysets[i * keyset_size + l];
			}
		}
		keyset_size = d;
	}

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





int keyset_is_valid(int ks)
{
	return 0 <= ks && ks < keyset_count && !!(keyuses[ks / BITU] & (1U << (ks % BITU)));
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

int keyset_add(int ks, const char *key)
{
	int idx;

	assert(key);
	assert(keyset_is_valid(ks));

	idx = get_tag(key, 1);
	if (idx < 0)
		return idx;

	keysets[ks * keyset_size + (idx / BITU)] |= 1U << (idx % BITU);
	return 0;
}

int keyset_sub(int ks, const char *key)
{
	int idx;

	assert(key);
	assert(keyset_is_valid(ks));

	idx = get_tag(key, 0);
	if (idx < 0)
		return idx;

	keysets[ks * keyset_size + (idx / BITU)] &= ~(1U << (idx % BITU));
	return 0;
}

int keyset_has(int ks, const char *key)
{
	int idx;

	assert(key);
	assert(keyset_is_valid(ks));

	idx = get_tag(key, 0);
	if (idx < 0)
		return 0;

	return !!(keysets[ks * keyset_size + (idx / BITU)] & (1U << (idx % BITU)));
}

int keyset_has_keyid(int ks, int kid)
{
	assert(keyset_is_valid(ks));
	assert(keyset_is_valid_keyid(kid));

	return !!(keysets[ks * keyset_size + (kid / BITU)] & (1U << (kid % BITU)));
}

int keyset_new()
{
	int n;
	void *p;

	if (!keyset_free) {
		n = keyset_count / BITU;
		p = realloc(keyuses, (n + 1) * sizeof * keyuses);
		if (!p)
			return -ENOMEM;
		keyuses = p;
		keyuses[n] = 0;
		p = realloc(keysets, (keyset_count + BITU) * keyset_size * sizeof * keysets);
		if (!p)
			return -ENOMEM;
		keysets = p;
		keyset_count += BITU;
		keyset_free = BITU;
	}

	while (!~keyuses[keyset_iter / BITU])
		keyset_iter = ((keyset_iter & ~(BITU - 1)) + BITU) % keyset_count;
	while (keyuses[keyset_iter / BITU] & (1U << (keyset_iter % BITU)))
		keyset_iter = (keyset_iter + 1) % keyset_count;

	memset(keysets + keyset_iter * keyset_size, 0, keyset_size * sizeof * keysets);
	keyuses[keyset_iter / BITU] |= (1U << (keyset_iter % BITU));
	keyset_free--;
	return keyset_iter;
}

void keyset_delete(int ks)
{
	assert(keyset_is_valid(ks));
	
	keyuses[ks / BITU] &= ~(1U << (ks % BITU));
	keyset_free++;
}

void keyset_for_all_key(void (*cb)(const char*,int,void*), void *extra)
{
	int i;

	for (i = 0 ; i < tag_count ; i ++)
		cb(tag_strings[i], i, extra);
}

void keyset_for_all_keyset(void (*cb)(int,void*), void *extra)
{
	int i;

	for (i = 0 ; i < keyset_count ; i ++)
		if (keyuses[i / BITU] & (1U << (i % BITU)))
			cb(i, extra);
}

void keyset_for_all(int ks, void (*cb)(const char*,int,void*), void *extra)
{
	int i;

	for (i = 0 ; i < tag_count ; i++)
		if (keysets[ks * keyset_size + (i / BITU)] & (1U << (i % BITU)))
			cb(tag_strings[i], i, extra);
}

