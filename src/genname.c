#include <assert.h>
#include "genname.h"

static const char *vowels[] = { 
	"a", "e", "i", "o", "u", "ai", "ia", "ei", "io", "ui"
};

static const char *consonants[] = {
	"b", "d", "f", "j", "k", "l", "m", "n", "p", "r", "s", 
	"t", "v", "x", "z", "bl", "br", "fr", "fl", "kl",
	"kr", "mb", "pl", "pr", "pt", "st", "sv", "tr" 
};

/*
Generate in 'buffer' a name for the given 'key'.
'length' is the count of characters in 'buffer'.

'key' MUST be a positive or null number.

At most 'length' characters (including the terminating null)
are copied to buffer.

'buffer' can be NULL if 'length'==0. In that case, the returned value
can be used to allocate a string.

Returns: the count of characters (excluding the terminating null)
of the generated name. That count can be creater than 'length'.
*/
int gennamein(int key, char *buffer, int length)
{
	const char *stack[64];
	const char *p;
	int i, r, n;

	assert(key >= 0);

	/* stacks the components */
	i = 0;
	for(;;) {
		assert(i < (sizeof stack / sizeof*stack));
		n = (int)(sizeof vowels / sizeof*vowels);
		stack[i++] = vowels[key % n];
		key /= n;
		if (key == 0)
			break;
		assert(i < (sizeof stack / sizeof*stack));
		n = (int)(sizeof consonants / sizeof*consonants);
		stack[i++] = consonants[key % n];
		key /= n;
		if (key == 0)
			break;
	}

	/* write the name to buffer */
	r = 0;
	while(i)
		for(p = stack[--i] ; *p ; p++, r++)
			if(r < length)
				buffer[r] = *p;

	/* append the terminating null */
	if(r < length)
		buffer[r] = 0;

	/* capitalizze the first letter */
	if (length > 0)
		buffer[0] &= ~' ';

	return r;
}

/*
Generate the name for the given 'key'.

'key' MUST be a positive or null number.

Returns: the generated name as a string (that have to be copied).

CAUTION: use static storage, protect the call if threading is used.
*/
const char *genname(int key)
{
	int r;
	static char name[50];

	assert(key >= 0);

	r = gennamein(key, name, (int)sizeof name);
	assert(r < (int)sizeof name);

	return name;
}

#ifdef TESTGENNAME
#include <stdlib.h>
#include <stdio.h>

void g(int n)
{
	int i, k;
	for(i=1;i<=n;i++) {
		k = rand();
		if (k < 0) k = -(k+1);
		printf("%s   [%d]\n",genname(k),k);
	}
}
	
int main(int argc, char **argv)
{
	g(10000);
	return 0;
}
#endif

