#include "ffilter.h"
#include <unistd.h>

typedef struct {
	FILE *r;
	FILE *w;
} ffilter_t;

static void skip_line(FILE *file) {
	while (!feof(file)) {
		if (fgetc(file) == '\n') { return; }
	}
}

static void catch_up(const ffilter_t filter, off_t pos) {
	while (ftello(filter.r) != pos) {
		fputc(fgetc(filter.r), filter.w);
	}
}

void ffilter(const char *path, ffilter_cb_t cb, void *user_info) {
	FILE *file = fopen(path, "r+");
	ffilter_t filter = {0, 0};

	while (!feof(file)) {
		off_t trim_start = ftello(file);
		bool should_filter = cb(file, user_info);

		skip_line(file);

		if (should_filter) {
			if (filter.r == NULL) {
				filter.r = fopen(path, "r");
				filter.w = fopen(path, "r+");
				fseeko(filter.r, trim_start, SEEK_SET);
				fseeko(filter.w, trim_start, SEEK_SET);
			}

			catch_up(filter, trim_start);
			fseeko(filter.r, ftello(file), SEEK_SET);
		}
	}

	if (filter.r) {
		catch_up(filter, ftello(file));
		ftruncate(fileno(filter.w), ftello(filter.w));

		fclose(filter.r);
		fclose(filter.w);
	}

}

