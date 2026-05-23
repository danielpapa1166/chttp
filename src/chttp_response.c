#include "chttp_internal.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#define CHTTP_HEADER_BUFSZ 256

/*
 * Serialise and send an HTTP/1.1 response on fd.
 *
 * The response status defaults to "200 OK" and content_type to
 * "application/octet-stream" when not provided by the caller.
 */
void chttp_send_response(int fd, const chttp_response_t *resp)
{
    const char *status       = (resp->status       && resp->status[0])       ? resp->status       : "200 OK";
    const char *content_type = (resp->content_type && resp->content_type[0]) ? resp->content_type : "application/octet-stream";

    char header[CHTTP_HEADER_BUFSZ];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status,
        content_type,
        resp->body_len);

    if (header_len <= 0 || (size_t)header_len >= sizeof(header))
        return;

    send(fd, header, (size_t)header_len, MSG_NOSIGNAL);

    if (resp->body && resp->body_len > 0)
        send(fd, resp->body, resp->body_len, MSG_NOSIGNAL);
}
