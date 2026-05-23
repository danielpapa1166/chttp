#include "chttp_internal.h"

#include <string.h>
#include <stdlib.h>

/*
 * Parse an HTTP/1.1 request from a null-terminated buffer.
 *
 * The buffer is modified in-place: NUL bytes are written to delimit tokens.
 * req->method, req->path, and req->body point into buf.
 *
 * Returns 0 on success, -1 on parse error.
 */
int chttp_parse_request(char *buf, size_t len, chttp_request_t *req)
{
  if (!buf || !req || len == 0) {
    return -1;
  }

  /* --- Request line: METHOD SP path SP HTTP/x.x CRLF --- */
  char *p = buf;

  /* Method */
  char *method_end = strchr(p, ' ');
  if (!method_end)
    return -1;
  *method_end = '\0';
  req->method = p;
  p = method_end + 1;

  /* Path (may contain query string) */
  char *path_end = strchr(p, ' ');
  if (!path_end)
    return -1;
  *path_end = '\0';

  /* Strip query string */
  char *qs = strchr(p, '?');
  if (qs)
    *qs = '\0';

  req->path = p;
  p = path_end + 1;

  /* Basic path safety: must start with '/', no '..' components */
  if (!chttp_path_is_safe(req->path))
    return -1;

  /* Skip to end of request line */
  char *crlf = strstr(p, "\r\n");
  if (!crlf)
    return -1;
  p = crlf + 2;

  /* --- Headers: scan for \r\n\r\n (header/body boundary) --- */
  size_t content_length = 0;
  char *headers_end = strstr(p, "\r\n\r\n");
  if (!headers_end)
    return -1;

  /* Scan Content-Length header (case-insensitive prefix match) */
  char *hdr = p;
  while (hdr < headers_end) {
    if (strncasecmp(hdr, "Content-Length:", 15) == 0) {
      hdr += 15;
      while (*hdr == ' ') {
        hdr++; 
      }
      content_length = (size_t)strtoul(hdr, NULL, 10);
      break;
    }
    char *next = strstr(hdr, "\r\n");
    if (!next || next >= headers_end)
      break;
    hdr = next + 2;
  }

  /* --- Body --- */
  char *body_start = headers_end + 4;
  size_t remaining = (size_t)(buf + len - body_start);

  if (content_length > 0 && remaining > 0) {
    req->body   = body_start;
    req->body_len = content_length < remaining ? content_length : remaining;
  } else {
    req->body   = NULL;
    req->body_len = 0;
  }

  return 0;
}
