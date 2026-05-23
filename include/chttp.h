#ifndef CHTTP_H
#define CHTTP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque server handle */
typedef struct chttp_server chttp_server_t;

typedef struct {
    const char *method;       /* "GET", "POST", … */
    const char *path;         /* decoded, query-string stripped */
    const char *body;         /* NULL if no body */
    size_t      body_len;
} chttp_request_t;

typedef struct {
    const char *status;       /* e.g. "200 OK", "404 Not Found" */
    const char *content_type; /* e.g. "application/json" */
    char       *body;         /* heap-allocated; library frees after send */
    size_t      body_len;
} chttp_response_t;

typedef void (*chttp_handler_fn)(const chttp_request_t *req,
                                 chttp_response_t      *resp,
                                 void                  *user_data);

/* Lifecycle */
chttp_server_t *chttp_server_create(uint16_t port);
void            chttp_server_destroy(chttp_server_t *srv);

/* Route registration – call before chttp_server_run() */
int  chttp_server_register_route(chttp_server_t  *srv,
                                 const char      *method,
                                 const char      *path,
                                 chttp_handler_fn handler,
                                 void            *user_data);

/* Blocking run – call from a dedicated thread */
void chttp_server_run(chttp_server_t *srv);

/* Thread-safe stop – signals chttp_server_run() to return */
void chttp_server_stop(chttp_server_t *srv);

#ifdef __cplusplus
}
#endif

#endif /* CHTTP_H */
