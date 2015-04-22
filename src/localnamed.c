#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/param.h>
#include <launch.h>
#include <uv/uv.h>

#include "ffilter.h"

#define assert_zero(cmd)                                           \
	do {                                                           \
		int r = cmd;                                               \
		if (r != 0) {                                              \
			fprintf(stderr, "sadness: " #cmd " returned %d\n", r); \
			abort();                                               \
		}                                                          \
	} while (0)

#define HOSTS_FILE "/etc/hosts"
#define LOCALNAME_IP "127.0.0.1"
#define LOCALNAME_COMMENT "# localname"
#define LOCALNAME_FORMAT LOCALNAME_IP " %*[^\n#] " LOCALNAME_COMMENT

static bool cleanup_cb(FILE *file, void *user_info) {
	(void)user_info;
	int nscanned = 0;
	fscanf(file, LOCALNAME_FORMAT " %n", &nscanned);
	return nscanned != 0;
}

static bool cleanup_conn_cb(FILE *file, void *user_info) {
	void *token;
	if (fscanf(file, LOCALNAME_FORMAT " %zd", &token) != 0) {
		return user_info == token;
	} else {
		return false;
	}
}

static void cleanup() {
	ffilter(HOSTS_FILE, cleanup_cb, NULL);
}

static void append_host(const char *host, void *token) {
	FILE *hfile = fopen("/etc/hosts", "a");
	fprintf(hfile, LOCALNAME_IP " %s " LOCALNAME_COMMENT " %zd\n", host, token);
	fclose(hfile);
}

// Not using UV's async filesystem APIs because we only want one edit to the
// hosts file at a time
static const char *handle_line(const char *line, void *token) {
	void (*op)(const char *, void *) = NULL;

	switch (line[0]) {
	case '+':
		op = append_host;
		break;
	case '-':
		break;
	default: return "Invalid operation, expected + or -";
	}

	const char *name = line + 1;
	if (name[0] == '\0') { return "Empty IP"; }

	op(name, token);
	return NULL;
}

// Limit the space clients can allocate
#define LOCALNAME_MAX_LINE 1024

typedef struct {
	char line[LOCALNAME_MAX_LINE];
	size_t length;
} localname_state_t;

static void localname_state_init(localname_state_t *state) {
	state->line[0] = '\0';
	state->length = 0;
}

static int localname_process_buf(localname_state_t *state, void *buf, size_t len, void *token) {
	size_t buf_consumed = 0;
	while (buf_consumed < len) {
		{
			// Leave room for a trailing \0
			size_t to_copy = MIN(LOCALNAME_MAX_LINE - 1 - state->length, len);
			if (to_copy == 0) {
				return -1;
			}
			memcpy(state->line + state->length, buf, to_copy);
			state->length += to_copy;
			buf_consumed += to_copy;
		}

		if (state->line[state->length - 1] == '\n') {
			state->line[state->length - 1] = '\0';
			const char *err = handle_line(state->line, token);
			if (err != NULL) {
				fprintf(stderr, "%s\n", err);
			}
			localname_state_init(state);
		}
	}
	return 0;
}

static int get_launchd_socket() {
	int* fds;
	size_t cnt = 0;

	if (launch_activate_socket("localnamed", &fds, &cnt) != 0) {
		fprintf(stderr, "Couldn't check in with launchd, exiting.\n");
		exit(1);
	}

	if (cnt == 0) {
		fprintf(stderr, "Didn't get a socket from launchd, exiting.\n");
		exit(1);
	}

	int fd = fds[0];
	free(fds);
	return fd;
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	(void)handle;
	buf->base = malloc(suggested_size);
	buf->len = suggested_size;
}

static int open_connections = 0;

static void on_close(uv_handle_t *handle) {
	free(handle->data);
	free(handle);
	ffilter(HOSTS_FILE, cleanup_conn_cb, handle);
	if (--open_connections == 0) {
		exit(0);
	}
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
	localname_state_t *state = stream->data;
	if (nread < 0 || localname_process_buf(state, buf->base, (size_t)nread, stream) != 0) {
		uv_close((uv_handle_t *)stream, on_close);
	}
	if (buf->base) { free(buf->base); }
}

static void on_connection(uv_stream_t* listening_pipe, int status) {
	assert_zero(status);
	open_connections++;

	uv_pipe_t *client = malloc(sizeof(uv_pipe_t));
	uv_pipe_init(uv_default_loop(), client, 0);

	assert_zero(uv_accept(listening_pipe, (uv_stream_t *)client));

	localname_state_t *state = malloc(sizeof(localname_state_t));
	localname_state_init(state);

	client->data = state;

	uv_read_start((uv_stream_t *)client, alloc_buffer, on_read);
}

static void __attribute__((noreturn)) on_sigterm(uv_signal_t *handle, int signum) {
	(void)handle;
	(void)signum;
	cleanup();
	exit(0);
}

int main() {
	int fd = get_launchd_socket();
	uv_pipe_t listening_pipe;

	uv_signal_t term_signal;
	uv_signal_init(uv_default_loop(), &term_signal);
	uv_signal_start(&term_signal, on_sigterm, SIGTERM);

	cleanup();

	uv_pipe_init(uv_default_loop(), &listening_pipe, 0);
	uv_pipe_open(&listening_pipe, fd);
	assert_zero(uv_listen(
		(uv_stream_t*)&listening_pipe, SOMAXCONN, on_connection
	));

	return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
