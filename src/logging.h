#include <stdio.h>

#define LOG_DEBUG(str) { fprintf(debug_fp, str); fflush(); }
#define LOG_MIGRATION(str)