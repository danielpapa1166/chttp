#include "chttp_internal.h"

#include <string.h>

/* --- MIME type lookup --- */

static const struct {
    const char *ext;
    const char *mime;
} mime_table[] = {
    { ".html", "text/html"              },
    { ".htm",  "text/html"              },
    { ".css",  "text/css"               },
    { ".js",   "application/javascript" },
    { ".json", "application/json"       },
    { ".txt",  "text/plain"             },
    { ".png",  "image/png"              },
    { ".jpg",  "image/jpeg"             },
    { ".jpeg", "image/jpeg"             },
    { ".gif",  "image/gif"              },
    { ".svg",  "image/svg+xml"          },
    { ".ico",  "image/x-icon"           },
    { NULL,    NULL                     }
};

/*
 * Return the MIME type string for the given file path based on its extension.
 * Falls back to "application/octet-stream" if the extension is not recognised.
 */
const char *chttp_mime_type(const char *path)
{
    if (!path)
        return "application/octet-stream";

    const char *dot = strrchr(path, '.');
    if (!dot)
        return "application/octet-stream";

    for (int i = 0; mime_table[i].ext != NULL; i++) {
        if (strcmp(dot, mime_table[i].ext) == 0)
            return mime_table[i].mime;
    }
    return "application/octet-stream";
}

/* --- Path safety validation --- */

/*
 * Return 1 if path is safe to use, 0 otherwise.
 *
 * A safe path:
 *   - Is non-NULL and non-empty
 *   - Starts with '/'
 *   - Contains no null bytes beyond the terminator (already guaranteed
 *     by the parser, but double-checked here)
 *   - Contains no ".." path components
 */
int chttp_path_is_safe(const char *path)
{
    if (!path || path[0] != '/')
        return 0;

    /* Reject embedded null bytes */
    const char *p = path;
    while (*p) {
        if ((unsigned char)*p == '\0')
            return 0;
        p++;
    }

    /* Reject ".." components */
    p = path;
    while (*p) {
        /* Match on "/../", "/..\0", or start "/../" */
        if (p[0] == '/' && p[1] == '.' && p[2] == '.') {
            if (p[3] == '/' || p[3] == '\0')
                return 0;
        }
        p++;
    }

    return 1;
}
