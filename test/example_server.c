#include "chttp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static chttp_server_t *g_srv = NULL;

static void on_signal(int sig)
{
    (void)sig;
    if (g_srv)
        chttp_server_stop(g_srv);
}

/* GET /ping  →  200 {"pong":true} */
static void handle_ping(const chttp_request_t *req,
                        chttp_response_t      *resp,
                        void                  *user_data)
{
    (void)req;
    (void)user_data;

    const char *msg   = "{\"pong\":true}";
    resp->status       = "200 OK";
    resp->content_type = "application/json";
    resp->body_len     = strlen(msg);
    resp->body         = strdup(msg);
}

/* GET /hello?  →  200 plain text greeting */
static void handle_hello(const chttp_request_t *req,
                         chttp_response_t      *resp,
                         void                  *user_data)
{
    (void)user_data;

    char buf[64];
    snprintf(buf, sizeof(buf), "Hello from chttp! You requested: %s", req->path);

    resp->status       = "200 OK";
    resp->content_type = "text/plain";
    resp->body_len     = strlen(buf);
    resp->body         = strdup(buf);
}

/* POST /echo  →  200, body mirrored back as plain text */
static void handle_echo(const chttp_request_t *req,
                        chttp_response_t      *resp,
                        void                  *user_data)
{
    (void)user_data;

    resp->status       = "200 OK";
    resp->content_type = "text/plain";

    if (req->body && req->body_len > 0) {
        resp->body     = strndup(req->body, req->body_len);
        resp->body_len = req->body_len;
    } else {
        const char *empty = "(no body)";
        resp->body     = strdup(empty);
        resp->body_len = strlen(empty);
    }
}

int main(void)
{
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    uint16_t port = 8080;
    g_srv = chttp_server_create(port);
    if (!g_srv) {
        fprintf(stderr, "chttp_server_create failed on port %u\n", port);
        return 1;
    }

    chttp_server_register_route(g_srv, "GET",  "/ping",  handle_ping,  NULL);
    chttp_server_register_route(g_srv, "GET",  "/hello", handle_hello, NULL);
    chttp_server_register_route(g_srv, "POST", "/echo",  handle_echo,  NULL);

    printf("chttp example server listening on 127.0.0.1:%u\n", port);
    printf("  GET  http://127.0.0.1:%u/ping\n", port);
    printf("  GET  http://127.0.0.1:%u/hello\n", port);
    printf("  POST http://127.0.0.1:%u/echo  (body echoed back)\n", port);
    printf("Press Ctrl-C to stop.\n");

    chttp_server_run(g_srv);

    chttp_server_destroy(g_srv);
    return 0;
}
