#include <glib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cmd.h>

struct test_run_cmd_data {
	struct cmd_req* req;
	struct cmd_res* res;
	char** envp;
};

static void setup(gpointer fixture, gconstpointer user_data) {
	struct test_run_cmd_data* data = (struct test_run_cmd_data*)user_data;
	data->req = g_new0(struct cmd_req, 1);
	data->res = g_new0(struct cmd_res, 1);

	data->req->in_fd = -1;
	data->req->env = data->envp;
	data->req->cwd = "/tmp";
}

static void teardown(gpointer fixture, gconstpointer user_data) {
	struct test_run_cmd_data* data = (struct test_run_cmd_data*)user_data;

	if (data->res->err != NULL)
		g_error_free(data->res->err);
	g_free(data->req);
	g_free(data->res);
}

static void test_run_exit_code(gpointer fixture, gconstpointer user_data) {
	struct cmd_req* req = ((struct test_run_cmd_data*)user_data)->req;
	struct cmd_res* res = ((struct test_run_cmd_data*)user_data)->res;

	req->cmd_string = "/bin/sh -c 'exit 0'";
	g_assert(run_cmd(res, req) == 0);
	g_assert_no_error(res->err);
	g_assert(res->exit_status == 0);

	req->cmd_string = "/bin/sh -c 'exit 1'";
	g_assert(run_cmd(res, req) == 0);
	g_assert_no_error(res->err);
	g_assert(res->exit_status == 1);
}

static void test_run_stdout(gpointer fixture, gconstpointer user_data) {
	struct cmd_req* req = ((struct test_run_cmd_data*)user_data)->req;
	struct cmd_res* res = ((struct test_run_cmd_data*)user_data)->res;

	req->cmd_string = "/bin/sh -c 'echo foo'";
	run_cmd(res, req);
	g_assert_no_error(res->err);
	g_assert(res->exit_status == 0);
	
	char* buf = g_malloc0(strlen("foo\n") + 1);
	g_assert(read(res->out_fd, buf, strlen("foo\n")) == strlen("foo\n"));
	g_assert_cmpstr(buf, ==, "foo\n");

	req->cmd_string = "/bin/sh -c 'exit 0'";
	run_cmd(res, req);
	g_assert_no_error(res->err);
	g_assert(res->exit_status == 0);
	
	memset(buf, 0, strlen("foo"));
	g_assert(read(res->out_fd, buf, 0) == 0);
	g_assert_cmpstr(buf, ==, "");
}

static void test_run_stderr(gpointer fixture, gconstpointer user_data) {
	struct cmd_req* req = ((struct test_run_cmd_data*)user_data)->req;
	struct cmd_res* res = ((struct test_run_cmd_data*)user_data)->res;

	req->cmd_string = "/bin/sh -c 'echo foo 1>&2'";
	run_cmd(res, req);
	g_assert_no_error(res->err);
	g_assert(res->exit_status == 0);
	
	char* buf = g_malloc0(strlen("foo\n") + 1);
	g_assert(read(res->err_fd, buf, strlen("foo\n")) == strlen("foo\n"));
	g_assert_cmpstr(buf, ==, "foo\n");

	req->cmd_string = "/bin/sh -c 'exit 0'";
	run_cmd(res, req);
	g_assert_no_error(res->err);
	g_assert(res->exit_status == 0);
	
	memset(buf, 0, strlen("foo"));
	g_assert(read(res->err_fd, buf, 0) == 0);
	g_assert_cmpstr(buf, ==, "");
}

static void test_run_err(gpointer fixture, gconstpointer user_data) {
	struct cmd_req* req = ((struct test_run_cmd_data*)user_data)->req;
	struct cmd_res* res = ((struct test_run_cmd_data*)user_data)->res;

	// Not going to test every error - just going to test that the functions
	// that produce GError's handle them appropriately
	req->cmd_string = "/bin/sh -c 'echo fail";
	run_cmd(res, req);
	g_assert_error(res->err, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING);

	res->err = NULL;
	req->cmd_string = "/blob/foobarbazzle";
	run_cmd(res, req);
	g_assert_error(res->err, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);

	res->err = NULL;
	req->cmd_string = "/bin/sh -c 'exit -0'";
	req->cwd = "/foobarbaz";
	run_cmd(res, req);
	g_assert_error(res->err, G_SPAWN_ERROR, G_SPAWN_ERROR_CHDIR);
}

static void test_run_null_cmd(gpointer fixture, gconstpointer user_data) {
	struct cmd_req* req = ((struct test_run_cmd_data*)user_data)->req;
	struct cmd_res* res = ((struct test_run_cmd_data*)user_data)->res;

	req->cmd_string = "";
	run_cmd(res, req);
	g_assert_error(res->err, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING);

	req->cmd_string = NULL;
	run_cmd(res, req);
	g_assert_error(res->err, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING);
}

static void test_construct_sudo_cmd(gpointer fixture, gconstpointer user_data) {
	struct cmd_req* req = ((struct test_run_cmd_data*)user_data)->req;

	req->cmd_string = "/bin/ls";
	gchar* res = construct_sudo_cmd(req);
	g_assert_cmpstr(res, ==, "/bin/ls");
	g_free(res);

	req->sudo = TRUE;
	res = construct_sudo_cmd(req);
	g_assert_cmpstr(res, ==, "sudo -u root /bin/ls");
	g_free(res);

	req->username = "worr";
	res = construct_sudo_cmd(req);
	g_assert_cmpstr(res, ==, "sudo -u worr /bin/ls");
	g_free(res);

	req->username = "";
	res = construct_sudo_cmd(req);
	g_assert_cmpstr(res, ==, "sudo -u root /bin/ls");
	g_free(res);

	req->username = " ";
	res = construct_sudo_cmd(req);
	g_assert_cmpstr(res, ==, "sudo -u root /bin/ls");
	g_free(res);

	req->cmd_string = "";
	res = construct_sudo_cmd(req);
	g_assert(res == NULL);

	req->cmd_string = NULL;
	res = construct_sudo_cmd(req);
	g_assert(res == NULL);
}

int main(int argc, char** argv, char** env) {
	g_test_init(&argc, &argv, NULL);

	struct test_run_cmd_data data;
	data.envp = env;

	g_test_add("/TestRunCmd/ConstructSudoCmd", void, &data, setup, test_construct_sudo_cmd, teardown);
	g_test_add("/TestRunCmd/ExitCode", void, &data, setup, test_run_exit_code, teardown);
	g_test_add("/TestRunCmd/Stdout", void, &data, setup, test_run_stdout, teardown);
	g_test_add("/TestRunCmd/Stderr", void, &data, setup, test_run_stderr, teardown);
	g_test_add("/TestRunCmd/Errors", void, &data, setup, test_run_err, teardown);
	g_test_add("/TestRunCmd/NullCmd", void, &data, setup, test_run_null_cmd, teardown);

	return g_test_run();
}
