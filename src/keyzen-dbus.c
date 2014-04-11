/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include "keyzen.h"
#include "keyzen-dbus.h"

struct cache_entry {
	char *name;
	pid_t pid;
};

static char get_pid_method[] = "GetConnectionUnixProcessID";
static int timeout = 200; /* a time out of 100 ms by default */
static struct cache_entry cache[3]; /* init to zero. only 3 entries, hard coded!! */

static int read_cache(const char *name, pid_t *pid)
{
	if (!cache[0].name) return 0;
	if (!strcmp(name, cache[0].name)) {*pid = cache[0].pid; return 1;}
	if (!cache[1].name) return 0;
	if (!strcmp(name, cache[1].name)) {*pid = cache[1].pid; return 1;}
	if (!cache[2].name) return 0;
	if (!strcmp(name, cache[2].name)) {*pid = cache[2].pid; return 1;}
	return 0;
}

static void add_cache(const char *name, pid_t pid)
{
	char *n;

	n = strdup(name);
	if (n) {
		free(cache[2].name);
		cache[2] = cache[1];
		cache[1] = cache[0];
		cache[0].name = n;
		cache[0].pid = pid;
	}
}

int keyzen_dbus_get_pid_of_name(DBusConnection *connection, const char *name, pid_t *pid)
{
	int result;
	DBusError error;
	DBusMessage *message, *reply;
	dbus_bool_t b;
    dbus_uint32_t u32;

	assert(connection);
	assert(name);
	assert(pid);

	/* look at cache */
	if (read_cache(name, pid))
		return 0;

	/* create the message */
	message = dbus_message_new_method_call(
				DBUS_SERVICE_DBUS, 
				DBUS_PATH_DBUS, 
				DBUS_INTERFACE_DBUS, 
				get_pid_method);

	if (message == NULL) {
		result = -ENOMEM; /* can't create the message */
	} else {
		/* fulfill the message with the name */
		b = dbus_message_append_args(
				message, 
				DBUS_TYPE_STRING, 
				&name, 
				DBUS_TYPE_INVALID);
		if (b != TRUE) {
			result = -ENOMEM; /* can't fullfill the message */
		} else {
			dbus_error_init(&error);
			reply = dbus_connection_send_with_reply_and_block(
							connection, 
							message, 
							timeout, 
							&error);
			if (!reply) {
				result = -ENOTSUP; /* got no reply */
			} else {
				if (dbus_message_get_type(reply)
						!= DBUS_MESSAGE_TYPE_METHOD_RETURN) {
					result = -ENOTSUP; /* unexpected answer kind */
				} else {
					dbus_error_init(&error);
					b = dbus_message_get_args(
								reply, 
								&error, 
								DBUS_TYPE_UINT32, 
								&u32, 
								DBUS_TYPE_INVALID);
					if (b != TRUE) {
						result = -ENOTSUP; /* unexpected argument types */
					} else {
						*pid = (pid_t)u32;
						result = 0;
						add_cache(name, (pid_t)u32);
					}
				}
				dbus_message_unref(reply);
			}
		}
		dbus_message_unref(message);
	}
	return result;
}

int keyzen_dbus_get_pid_of_sender(DBusConnection *connection, DBusMessage *message, pid_t *pid)
{
	return keyzen_dbus_get_pid_of_name(connection, dbus_message_get_sender(message), pid);
}

int keyzen_dbus_sender_has_keys(DBusConnection *connection, DBusMessage *message, const char **keys, int count)
{
	int result;
	pid_t pid;

	result = keyzen_dbus_get_pid_of_sender(connection, message, &pid);
	if (!result)
		result = keyzen_process_has_keys(pid, keys, count);

	return result;
}

int keyzen_dbus_sender_has_key(DBusConnection *connection, DBusMessage *message, const char *key)
{
	return keyzen_dbus_sender_has_keys(connection, message, &key, 1);
}



