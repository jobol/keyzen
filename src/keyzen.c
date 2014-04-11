

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "keyzen.h"

struct item {
	char *entry;
	int flag;
};

#define FLAG_NONE   0
#define FLAG_EXIST  1
#define FLAG_KEEP   2

static char mountpoint[PATH_MAX];
static int mountlength = 0;
static int mpidlength = 0;
static int mkeylength = 0;
static char devname[] = KEYZEN_FS_KEY;
static char self[] = KEYZEN_SELF_NAME;
static char adminkey[] = KEYZEN_ADMIN_KEY;

static size_t entries_array_count = 0;
static size_t entries_array_alloc = 0;
static struct item *entries_array = 0;
static size_t entries_data_count = 0;
static size_t entries_data_alloc = 0;
static char *entries_data = 0;

static int detect_keyzen_mount_point(const char *dev, char *path, int pathlen)
{
	char buffer[8192];
	int file;
	int result;
	int rlen;
	int pos;
	int state;
	int len;

	/* open the file */
	file = open("/proc/self/mounts", O_RDONLY);
	if (file < 0)
		result = -errno;
	else {
		/* read the file */
		len = 0;
		state = 0;
		result = 0;
		do {
			rlen = (int)read(file, buffer, sizeof buffer);
			if (rlen < 0)
				result = -errno;
			else if (rlen == 0)
				result = -ENOTSUP; /* no fullfill case state == -1 because trailing fields */
			else {
				pos = 0;
				while(pos < rlen && !result) {
					switch (state) {
					case -1:

						while (pos < rlen) {
							if (len >= pathlen) {
								result = -ENAMETOOLONG;
								break;
							} else if (buffer[pos] == ' ') {
								result = len;
								buffer[len] = 0;
								break;
							} else {
								path[len++] = buffer[pos++];
							}
						}
						break;

					case -2:
						while (pos < rlen) {
							if (buffer[pos++] == '\n') {
								state = 0;
								break;
							}
						}
						break;

					default:
						while (pos < rlen) {
							if (buffer[pos] == dev[state]) {
								pos++;
								state++;
							} else if (!dev[state] && buffer[pos] == ' ') {
								pos++;
								state = -1;
								break;
							} else {
								state = -2;
								break;
							}
						}
						break;
					}
				}
			}
		} while (!result);
		/* close and return */
		close(file);
	}

	return result;
}

static int ensure_mount_point()
{
	if (mountlength == 0)
		mountlength = detect_keyzen_mount_point(devname, mountpoint, (int)sizeof mountpoint);
	return mountlength < 0 ? mountlength : 0;
}

static void itoa(int value, char *buffer)
{
	int i;
	char c;

	if (!value) {
		buffer[0] = '0';
		buffer[1] = 0;
	} else {
		if (value < 0) {
			/* handling case of INTEGER_MINIMUM that has no positive value */
			value = -value;
			buffer[0] = '-';
			buffer[1] = '0' + (char)(((unsigned)value) % 10);
			value = (int)(((unsigned)value) / 10);
			i = 2;
		} else {
			i = 0;
		}
		while (value) {
			buffer[i++] = '0' + (char)(value % 10);
			value /= 10;
		}
		buffer[i--] = 0;
		while (value < i) {
			c = buffer[value];
			buffer[value++] = buffer[i];
			buffer[i--] = c;
		}
	}
}

static const char *pidstr(pid_t pid)
{
	static char buffer[20];

	itoa((int)pid, buffer);
	return buffer;
}

static int append_path(const char *item, int base)
{
	int i;

	assert(item);

	i = base;
	mountpoint[i++] = '/';
	while (*item) {
		if (i >= (int)sizeof mountpoint)
			return -ENAMETOOLONG;
		mountpoint[i++] = *item++;
	}
	if (i >= (int)sizeof mountpoint)
		return -ENAMETOOLONG;

	mountpoint[i] = 0;
	return i;
}

static int set_pid_path(const char *pid)
{
	mpidlength = append_path(pid, mountlength);
	return mpidlength;
}

static int set_key_path(const char *key)
{
	mkeylength = append_path(key, mpidlength);
	return mkeylength;
}

static void clear_entries()
{
	entries_array_count = 0;
	entries_data_count = 0;
}

static int get_entry(const char *entry, int create, int flag)
{
	int l, u, i, s;
	char *data;
	void *ptr;
	size_t alloc;
	size_t count;
	size_t len;

	assert(entry);
	/* refuse entries named "", "." and ".." */
	if (!entry[0])
		return -EINVAL;
	if (entry[0] == '.') {
		if (!entry[1])
			return -EINVAL;
		if (entry[1] == '.' && !entry[2])
			return -EINVAL;
	}

	/* search entry index */
	l = 0;
	u = entries_array_count;
	while (l < u) {
		i = (l + u) >> 1;
		s = strcmp(entry, entries_array[i].entry);
		if (s == 0)
			return i;
		if (s < 0)
			u = i;
		else
			l = i + 1;
	}

	/* not found */
	if (!create)
		return -ENOENT;

	/* prepare the entry data */
	len = 1 + strlen(entry);
	count = entries_data_count + len;
	alloc = entries_data_alloc;
	while (count > alloc)
		alloc = alloc < 1000 ? 1000 : 2*alloc;
	if (alloc != entries_data_alloc) {
		ptr = realloc(entries_data, alloc * sizeof * entries_data);
		if (!ptr)
			return -ENOMEM;
		entries_data = ptr;
		entries_data_alloc = alloc;
	}

	/* prepare the entry array */
	alloc = entries_array_alloc;
	while (entries_array_count >= alloc)
		alloc = alloc < 30 ? 30 : 2*alloc;
	if (alloc != entries_array_alloc) {
		ptr = realloc(entries_array, alloc * sizeof * entries_array);
		if (!ptr)
			return -ENOMEM;
		entries_array = ptr;
		entries_array_alloc = alloc;
	}

	/* insert the data */
	u = entries_array_count++;
	while (u > l) {
		i = u - 1;
		entries_array[u] = entries_array[i];
		u = i;
	}
	data = entries_data + entries_data_count;
	entries_array[u].entry = data;
	memcpy(data, entry, len);
	entries_data_count = count;
	entries_array[u].flag = flag;
	return u;
}

static int internal_list_keys(const char *pid)
{
	int result;
	int sts;
	DIR *dir;
	struct dirent *entry;

	clear_entries();

	result = ensure_mount_point();
	if (result < 0)
		return result;

	result = set_pid_path(pid);
	if (result < 0)
		return result;

	dir = opendir(mountpoint);
	if (!dir)
		return -errno;

	result = 0;
	errno = 0;
	entry = readdir(dir);
	while (entry && !result) {
		if (entry->d_type == DT_REG) {
			sts = get_entry(entry->d_name, 1, FLAG_EXIST);
			if (sts < 0)
				result = sts;
		}
		entry = readdir(dir);
	}
	if (!result && errno)
		result = -errno;

	closedir(dir);

	return result;
}

static int internal_export_list_keys(const char *pid, void **list)
{
	int result;
	int *data;
	int n;
	int i;
	int o;

	result = internal_list_keys(pid);
	if (result < 0)
		return result;

	data = malloc((1+entries_array_count) * sizeof(int) + entries_data_count);
	if (!data)
		return -ENOMEM;

	n = entries_array_count;
	o = (n+1) * sizeof(int);
	data[0] = n;
	for (i = 1 ; i <= n ; i++)
		data[i] = o + (int)(entries_array[i-1].entry - entries_data);
	memcpy(data+i, entries_data, entries_data_count);
	*list = data;
	return 0;
}

static int internal_has_keys(const char *pid, const char **keys, int count)
{
	int i, result;

	result = ensure_mount_point();
	if (result < 0)
		return result;

	result = set_pid_path(pid);
	if (result < 0)
		return result;

	for (i = 0 ; i < count ; i++) {
		result = set_key_path(keys[i]);
		if (result < 0)
			return result;

		result = access(mountpoint, F_OK);
		if (result < 0)
			return -errno;
	}

	return 0;
}












static int internal_add_key(const char *key)
{
	int result;

	result = set_key_path(key);
	if (result < 0)
		return result;

	result = mknod(mountpoint, S_IFREG, 0);
	if (result < 0 && errno != EEXIST)
		return -errno;

	return 0;
}

static int internal_drop_key(const char *key)
{
	int result;

	result = set_key_path(key);
	if (result < 0)
		return result;

	result = unlink(mountpoint);
	if (result < 0 && errno != ENOENT)
		return -errno;

	return 0;
}






int keyzen_process_has_keys(pid_t pid, const char **keys, int count)
{
	return internal_has_keys(pidstr(pid), keys, count);
}

int keyzen_process_list_keys(pid_t pid, void **list)
{
	return internal_export_list_keys(pidstr(pid), list);
}

int keyzen_self_has_keys(const char **keys, int count)
{
	return internal_has_keys(self, keys, count);
}

int keyzen_self_add_keys(const char **keys, int count)
{
	int i, result;

	result = ensure_mount_point();
	if (result < 0)
		return result;

	result = set_pid_path(self);
	if (result < 0)
		return result;

	result = 0;
	for (i = 0 ; !result && i < count ; i++)
		result = internal_add_key(keys[i]);

	return result;
}

int keyzen_self_drop_keys(const char **keys, int count)
{
	int i, result, dropadmin;

	result = ensure_mount_point();
	if (result < 0)
		return result;

	result = set_pid_path(self);
	if (result < 0)
		return result;

	result = 0;
	dropadmin = 0;
	for (i = 0 ; !result && i < count ; i++) {
		if (!strcmp(keys[i], adminkey)) {
			dropadmin = 1;
		} else {
			result = internal_drop_key(keys[i]);
		}
	}

	if (!result && dropadmin)
		result = internal_drop_key(adminkey);
		
	return result;
}

int keyzen_self_set_keys(const char **keys, int count)
{
	int i, result, dropadmin;

	result = internal_list_keys(self);
	if (result < 0)
		return result;

	/* mark the entries to keep */
	for (i = 0 ; i < count ; i++) {
		result = get_entry(keys[i], 1, 0);
		if (result < 0)
			return result;

		entries_array[result].flag |= FLAG_KEEP;
	}

	result = 0;
	dropadmin = 0;
	for (i = 0 ; !result && i < entries_array_count ; i++) {
		switch (entries_array[i].flag & (FLAG_KEEP|FLAG_EXIST)) {
		case FLAG_EXIST:
			if (!strcmp(entries_array[i].entry, adminkey)) {
				dropadmin = 1;
			} else {
				result = internal_drop_key(entries_array[i].entry);
			}
			break;
		case FLAG_KEEP:
			result = internal_add_key(entries_array[i].entry);
			break;
		}
	}
	if (!result && dropadmin)
		result = internal_drop_key(adminkey);
		
	return result;
}


int keyzen_process_has_key(pid_t pid, const char *key)
{
	return keyzen_process_has_keys(pid, &key, 1);
}

int keyzen_self_has_key(const char *key)
{
	return keyzen_self_has_keys(&key, 1);
}

int keyzen_self_add_key(const char *key)
{
	return keyzen_self_add_keys(&key, 1);
}

int keyzen_self_drop_key(const char *key)
{
	return keyzen_self_drop_keys(&key, 1);
}

int keyzen_is_self_admin()
{
	return keyzen_self_has_key(adminkey);
}


int keyzen_self_list_keys(void **list)
{
	return internal_export_list_keys(self, list);
}

int keyzen_list_keys_count(void *list)
{
	assert(list);
	return *(int*)list;
}

char *keyzen_list_keys_name(void *list, int index)
{
	int *ints;

	assert(list);

	ints = list;
	if (index < 1 || index > *ints)
		return 0;

	return ((char*)list)+ints[index];
}

