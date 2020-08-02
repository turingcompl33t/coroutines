// trace.hpp
// Simple tracing helper.

#ifndef TRACE_H
#define TRACE_H

#include <cstdio>

#define trace(s) fprintf(stdout, "[%s] %s\n", __func__, s)

#endif // TRACE_H