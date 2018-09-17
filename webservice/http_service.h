#ifndef _TIGERA_HTTP_SERVICE__H__
#define _TIGERA_HTTP_SERVICE__H__

#include "includes.h"

/**
 * Configures http service.
 *
 * Should be called before http_service_start.
 *
 * @param nworkers # worker threads
 * @param sockaddr listening address and port for workers
 * @return 0, if successfull. 0, otherwise.
 */
int
http_service_init(int nworkers, struct sockaddr_storage *sockaddr);

/**
 * Start http worker threads defined by
 * http_service_init and blocks until process
 * receives and processes SIGTERM or SIGINT.
 *
 * SIGTERM or SIGINT triggers http service
 * finalization.
 * 
 * @return 0, if successfull. 0, otherwise.
 */
int
http_service_start(void);

/**
 * Cleanup http service.
 *
 * Should be called after http_service_start.
 */
void
http_service_fini(void);

/**
 * Remove session from http_service's queue.
 */
struct session;
void
http_service_session_remove(struct session *session);

#endif /* _TIGERA_HTTP_SERVICE__H__ */
