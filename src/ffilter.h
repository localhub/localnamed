#pragma once

#include <stdio.h>
#include <stdbool.h>

typedef bool ffilter_cb_t(FILE *file, void *);

void ffilter(const char *path, ffilter_cb_t filter, void *user_info);
