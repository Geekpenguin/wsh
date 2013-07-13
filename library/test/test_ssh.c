#include <glib.h>
#include <libssh/libssh.h>
#include <stdlib.h>
#include <stdio.h>

#include "ssh.h"

extern GQuark wsh_ssh_error_quark(void);

static const gchar* username = "worr";
static const gchar* password = "pass";
//static const gchar* remote = "127.0.0.1";
static const gint 	port = 22;
static const gchar* unreachable_remote = "xxx.yyy.zzz";

static void host_not_reachable(void) {
	ssh_session channel = NULL;
	fprintf(stderr, "Pointer: %p\n", channel);
	GError* err;
	gint ret = wsh_ssh_host(&channel, username, password, unreachable_remote, port, FALSE, &err);
	g_assert(ret == -1);
	g_assert(channel == NULL);
	g_assert_error(err, wsh_ssh_error_quark(), 4);
}

int main(int argc, char** argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/Library/SSH/UnreachableHost", host_not_reachable);

	return g_test_run();
}