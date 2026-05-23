#ifndef CHTTP_INTERNAL_H
#define CHTTP_INTERNAL_H

#include "chttp.h"
#include <stddef.h>

/* Parser */
int chttp_parse_request(char *buf, size_t len, chttp_request_t *req);

/* Response serialisation */
void chttp_send_response(int fd, const chttp_response_t *resp);

/* Utilities */
const char *chttp_mime_type(const char *path);
int         chttp_path_is_safe(const char *path);

#endif /* CHTTP_INTERNAL_H */
