#include <cstdlib>
#include <sys/param.h>
#include <launch.h>
#include <team/net.h>

#include "ffilter.h"

#define HOSTS_FILE "/etc/hosts"
#define LOCALNAME_IP "127.0.0.1"
#define LOCALNAME_COMMENT "# localname"
#define LOCALNAME_FORMAT LOCALNAME_IP " %*[^\n ] " LOCALNAME_COMMENT

static void cleanup() {
	ffilter(HOSTS_FILE, [](FILE *file) {
		int nscanned = 0;
		fscanf(file, LOCALNAME_FORMAT " %n", &nscanned);
		return nscanned != 0;
	});
}

static void append_host(const char *host, void *token) {
	FILE *hfile = fopen("/etc/hosts", "a");
	fprintf(hfile, LOCALNAME_IP " %s " LOCALNAME_COMMENT " %zd\n", host, token);
	fclose(hfile);
}

static void remove_host(const char *name, void *token) {
	ffilter(HOSTS_FILE, [&](FILE *file) {
		char lname[1024];
		void *ltoken;
		if (fscanf(file, LOCALNAME_IP " %1024[^\n ] " LOCALNAME_COMMENT " %zd", lname, &ltoken) != 2) {
			return false;
		}
		return token == ltoken && strcmp(name, lname) == 0;
	});
}

// Using synchronous filesystem APIs because we only want to edit the hosts
// file once at a time
static const char *handle_line(const char *line, void *token) {
	void (*op)(const char *, void *) = NULL;

	switch (line[0]) {
	case '+':
		op = append_host;
		break;
	case '-':
		op = remove_host;
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

struct localname_state_t {
	char line[LOCALNAME_MAX_LINE];
	size_t length;

	localname_state_t() : line{""}, length{0} {}
};

static bool localname_process_buf(localname_state_t *state, char *buf, size_t len, void *token) {
	while (len != 0) {
		{
			// Leave room for a trailing \0
			size_t to_copy = 0;
			while (to_copy < MIN(LOCALNAME_MAX_LINE - 1 - state->length, len)) {
				if (buf[to_copy++] == '\n') { break; }
			}
			memcpy(state->line + state->length, buf, to_copy);
			state->length += to_copy;
			buf += to_copy;
			len -= to_copy;
		}

		if (state->line[state->length - 1] == '\n') {
			state->line[state->length - 1] = '\0';
			if (auto err = handle_line(state->line, token)) {
				fprintf(stderr, "%s\n", err);
				return false;
			}
			*state = localname_state_t{};
		}
	}
	return true;
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

static int open_connections = 0;

static void __attribute__((noreturn)) on_sigterm(uv_signal_t *handle, int signum) {
	(void)handle;
	(void)signum;
	cleanup();
	exit(0);
}

int main() {
	int fd = get_launchd_socket();
	team::pipe<> listening_pipe;

	uv_signal_t term_signal;
	uv_signal_init(uv_default_loop(), &term_signal);
	uv_signal_start(&term_signal, on_sigterm, SIGTERM);

	cleanup();

	listening_pipe.open(fd);
	listening_pipe.listen(SOMAXCONN);

	listening_pipe.accept<team::pipe<>>([] (std::unique_ptr<team::pipe<>> client) {
		open_connections++;
		localname_state_t state;

		while (auto buf = client->read()) {
			if (!localname_process_buf(&state, static_cast<char *>(buf->base), buf->len, client.get())) {
				break;
			}
		}

		ffilter(HOSTS_FILE, [&] (FILE *file) {
			void *token;
			if (fscanf(file, LOCALNAME_FORMAT " %zd", &token) == 0) {
				return false;
			}
			return client.get() == token;
		});

		if (--open_connections == 0) {
			exit(0);
		}
	});
}
