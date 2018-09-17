#ifndef _TIGERA_IO_SERVICE__H__
#define _TIGERA_IO_SERVICE__H__

/* Interface for io services */

#include "io_channel.h"

typedef void (*io_service_accept_cb_t)(io_channel_t *, io_channel_accept_param_t *);
typedef void (*io_service_read_cb_t)(io_channel_t *);
typedef void (*io_service_write_cb_t)(io_channel_t *);
typedef void (*io_service_event_cb_t)(io_channel_t *, io_channel_event_t);

typedef struct io_service io_service_t;

struct io_service {
    io_service_accept_cb_t accept_cb;
    io_service_read_cb_t read_cb;
    io_service_write_cb_t write_cb;
    io_service_event_cb_t event_cb;
};


#endif /* _TIGERA_IO_SERVICE__H__ */
