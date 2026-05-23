#include "chttp.h"
#include "chttp_internal.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CHTTP_MAX_ROUTES   64
#define CHTTP_BACKLOG      8
#define CHTTP_RECV_BUFSZ   8192
#define CHTTP_MAX_BODY_LEN 65536

typedef struct {
    char             method[16];
    char             path[256];
    chttp_handler_fn handler;
    void            *user_data;
} chttp_route_t;

struct chttp_server {
    int           listen_fd;
    uint16_t      port;
    volatile int  stop_flag;
    chttp_route_t routes[CHTTP_MAX_ROUTES];
    int           route_count;
};

chttp_server_t *chttp_server_create(uint16_t port)
{
    chttp_server_t *srv = calloc(1, sizeof(*srv));
    if (!srv)
        return NULL;

    srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd < 0) {
        free(srv);
        return NULL;
    }

    int opt = 1;
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv->listen_fd);
        free(srv);
        return NULL;
    }

    if (listen(srv->listen_fd, CHTTP_BACKLOG) < 0) {
        close(srv->listen_fd);
        free(srv);
        return NULL;
    }

    srv->port = port;
    return srv;
}

void chttp_server_destroy(chttp_server_t *srv)
{
    if (!srv)
        return;
    if (srv->listen_fd >= 0)
        close(srv->listen_fd);
    free(srv);
}

int chttp_server_register_route(chttp_server_t  *srv,
                                const char      *method,
                                const char      *path,
                                chttp_handler_fn handler,
                                void            *user_data)
{
    if (!srv || !method || !path || !handler)
        return -1;
    if (srv->route_count >= CHTTP_MAX_ROUTES)
        return -1;

    chttp_route_t *r = &srv->routes[srv->route_count];
    strncpy(r->method, method, sizeof(r->method) - 1);
    strncpy(r->path,   path,   sizeof(r->path)   - 1);
    r->handler   = handler;
    r->user_data = user_data;
    srv->route_count++;
    return 0;
}

static chttp_route_t *find_route(chttp_server_t *srv,
                                 const char *method,
                                 const char *path)
{
    for (int i = 0; i < srv->route_count; i++) {
        chttp_route_t *r = &srv->routes[i];
        if (strcmp(r->method, method) == 0 && strcmp(r->path, path) == 0)
            return r;
    }
    return NULL;
}

static void handle_connection(chttp_server_t *srv, int conn_fd)
{
    char buf[CHTTP_RECV_BUFSZ];
    ssize_t n = recv(conn_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(conn_fd);
        return;
    }
    buf[n] = '\0';

    chttp_request_t req;
    memset(&req, 0, sizeof(req));

    if (chttp_parse_request(buf, (size_t)n, &req) != 0) {
        const char *bad_req = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(conn_fd, bad_req, strlen(bad_req), MSG_NOSIGNAL);
        close(conn_fd);
        return;
    }

    if (req.body_len > CHTTP_MAX_BODY_LEN) {
        const char *too_large = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(conn_fd, too_large, strlen(too_large), MSG_NOSIGNAL);
        close(conn_fd);
        return;
    }

    chttp_route_t *route = find_route(srv, req.method, req.path);

    chttp_response_t resp;
    memset(&resp, 0, sizeof(resp));

    if (route) {
        route->handler(&req, &resp, route->user_data);
    } else {
        resp.status       = "404 Not Found";
        resp.content_type = "text/plain";
        resp.body         = strdup("Not Found");
        resp.body_len     = 9;
    }

    chttp_send_response(conn_fd, &resp);
    free(resp.body);
    close(conn_fd);
}

void chttp_server_run(chttp_server_t *srv)
{
    if (!srv)
        return;

    srv->stop_flag = 0;

    while (!srv->stop_flag) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int conn_fd = accept(srv->listen_fd,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (conn_fd < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK)
                continue;
            break;
        }
        handle_connection(srv, conn_fd);
    }
}

void chttp_server_stop(chttp_server_t *srv)
{
    if (!srv)
        return;
    srv->stop_flag = 1;
    shutdown(srv->listen_fd, SHUT_RDWR);
}
