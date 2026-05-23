# chttp — Embedded HTTP Server Library: Architecture

## Overview

`chttp` is a lightweight, dependency-free embedded HTTP/1.1 server library written in C.
It provides the generic transport and routing layer; application-specific endpoint logic stays
in each consumer project.

Intended consumers:

| Project | Purpose |
|---------|---------|
| **config-server** | Serves a WiFi/API configuration UI during device setup |
| **application-manager** | Exposes a status and control interface for debugging and runtime management |

---

## Layered Design

```
┌─────────────────────────────────────────────────────────┐
│                   Consumer project                      │
│                                                         │
│   Route handlers (GET /status, POST /apps/:name/stop …) │
│   Application data access, business logic               │
└────────────────────────┬────────────────────────────────┘
                         │  registers routes / calls run
┌────────────────────────▼────────────────────────────────┐
│                     bkk-http                            │
│                                                         │
│   Socket setup & accept loop                            │
│   HTTP request parsing  (method, path, body)            │
│   Route dispatch        (method + path → callback)      │
│   HTTP response builder (status line, headers, body)    │
│   MIME type resolution                                  │
│   Path safety validation  (null bytes, traversal)       │
└─────────────────────────────────────────────────────────┘
                         │  POSIX / libc only
              (no external library dependencies)
```

### What belongs in the library

- TCP socket lifecycle: `socket()`, `setsockopt()`, `bind()`, `listen()`, `accept()`
- Raw request reading and null-termination
- Request-line parsing: method, path, HTTP version
- Header skipping (body boundary detection via `\r\n\r\n`)
- Body extraction for POST/PUT requests
- Route table: method + exact path → `http_handler_fn` callback
- Query-string stripping before route lookup
- Response serialisation: status line, `Content-Type`, `Content-Length`, `Connection: close`, body
- MIME type lookup (`.html`, `.css`, `.js`)
- Path safety checks (no `..` traversal, must start with `/`)
- Static file serving helper (optional, off by default)

### What stays in the consumer project

- All endpoint handler functions and their business logic
- Application data structures and synchronisation primitives
- Any external library usage (cJSON, wpa_cli, logging)
- Configuration (port number, bind address, static file root)
- Concurrency model (see below)

---

## Public API

```c
/* Opaque server handle */
typedef struct chttp_server chttp_server_t;

/* Handler callback signature.
 * req  – parsed request (method, path, body, body_len)
 * resp – caller fills this to build the response
 */
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
```

`user_data` is passed through to each handler call, allowing thread-safe access to
application state without global variables.

---

## Concurrency Model Per Consumer

### config-server — fork-per-request

config-server is a single-threaded process. It spawns a child process per accepted
connection, which matches the original implementation and is appropriate because:

- Connections are infrequent (one configuration session at a time)
- Handlers block for seconds (wpa_cli network validation)
- No shared in-process state needs protecting

```
main process
└── accept() loop
    └── fork() on each connection
        └── child: call handler → send response → exit
```

`http_server_run()` implements this loop internally when built in fork mode.

### application-manager — single dedicated thread

application-manager is multi-threaded and forking inside a multi-threaded process
is unsafe (only the forking thread survives in the child; mutexes held by other
threads become permanently locked). The HTTP server therefore runs as a single
blocking thread inside the AM process:

```
main thread          supervisor thread      http thread
    │                      │                   │
    │  pthread_create ─────┼───────────────►   │
    │                      │            accept() loop
    │                      │            one request at a time
    │                      │                   │
    │                      │◄── cmd pipe ───────┤  (control requests)
    │                      │                   │
```

- One request is handled at a time (sufficient for a debug/control dashboard)
- `http_server_run()` runs the accept loop; no forking occurs
- Handlers access `app_info_list` through a `pthread_rwlock_t` (read lock for
  status queries, write lock for control commands)
- Control commands (stop/start/restart) are not executed inline by the HTTP
  handler; instead the handler writes a command struct to a pipe that the
  supervisor thread drains in its `ppoll()` loop

#### Supervisor loop change required

The current supervisor uses a blocking `sigwaitinfo()`. To support the command
pipe it must move to `ppoll()` multiplexing both the signal mask and the read end
of the command pipe:

```c
struct pollfd fds[2];
fds[0].fd     = cmd_pipe_read_fd;
fds[0].events = POLLIN;
// fds[1] reserved for future additions

while (1) {
    ppoll(fds, 1, NULL, &sigchld_mask);   // wakes on SIGCHLD or pipe data
    // drain signal queue
    // drain command queue
}
```

---

## Repository Structure

```
chttp/
├── include/
│   └── chttp.h              public API header
├── src/
│   ├── chttp_server.c       socket lifecycle, accept loop, dispatch
│   ├── chttp_parser.c       request-line and body parsing
│   ├── chttp_response.c     response serialisation
│   └── chttp_utils.c        MIME types, path safety
├── CMakeLists.txt           builds static library: libchttp.a
└── README.md
```

Consumed as a CMake subdirectory or via `FetchContent`:

```cmake
FetchContent_Declare(chttp
    GIT_REPOSITORY https://github.com/your-org/chttp.git
    GIT_TAG        main)
FetchContent_MakeAvailable(chttp)

target_link_libraries(application_manager PRIVATE chttp)
target_link_libraries(config-server       PRIVATE chttp)
```

---

## Security Considerations

- Bind to `127.0.0.1` only; never `0.0.0.0` unless explicitly required
- Validate `Content-Length` before allocating the body buffer; reject requests
  exceeding a configured maximum (e.g. 64 KB)
- The path safety check (no `..`, must start with `/`) must run before any
  filesystem access
- The AM control interface accepts commands that affect running processes; any
  future authentication layer (e.g. Unix socket with filesystem permissions, or
  a token header) should be added at the route-registration level, not scattered
  across handlers
- Avoid `system()` and `popen()` in handlers; prefer direct syscalls or
  dedicated helper functions to prevent shell injection
