#ifndef _TIGERA_TCP_SOCKET__H__
#define _TIGERA_TCP_SOCKET__H__

/* Implementation of TCP socket IO channel */

#include "io_channel.h"

io_channel_t *
tcp_socket_new(void);

#endif
