#ifndef _TIGERA_SESSION__H__
#define _TIGERA_SESSION__H__

typedef struct session session_t;

void
session_free(session_t *session);

struct io_channel;

session_t *
session_new(struct io_channel *channel);

#endif /* _TIGERA_SESSION__H__ */
