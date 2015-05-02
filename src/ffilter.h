#pragma once

#include <cstdio>
#include <functional>

void ffilter(const char *path, std::function<bool(FILE *)> filter);
