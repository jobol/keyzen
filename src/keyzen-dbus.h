/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#ifndef KEYZEN_DBUS_H
#define KEYZEN_DBUS_H

int keyzen_dbus_get_pid_of_name(DBusConnection *connection, const char *name, pid_t *pid);
int keyzen_dbus_get_pid_of_sender(DBusConnection *connection, DBusMessage *message, pid_t *pid);
int keyzen_dbus_sender_has_keys(DBusConnection *connection, DBusMessage *message, const char **keys, int count);
int keyzen_dbus_sender_has_key(DBusConnection *connection, DBusMessage *message, const char *key);

#ifdef DBUS_GLIB_H /* GLIB binding */
#include <dbus/dbus-glib-lowlevel.h>
#define keyzen_dbus_g_get_pid_of_name(c,n,p)   \
			keyzen_dbus_get_pid_of_name(dbus_g_connection_get_connection(c),n,p)
#define keyzen_dbus_g_get_pid_of_sender(c,m,p) \
			keyzen_dbus_get_pid_of_sender(dbus_g_connection_get_connection(c),dbus_g_message_get_message(m),p)
#define keyzen_dbus_g_sender_has_keys(c,m,k,n) \
			keyzen_dbus_sender_has_keys(dbus_g_connection_get_connection(c),dbus_g_message_get_message(m),k,n)
#define keyzen_dbus_g_sender_has_key(c,m,k) \
			keyzen_dbus_sender_has_key(dbus_g_connection_get_connection(c),dbus_g_message_get_message(m),k)
#endif

#endif

