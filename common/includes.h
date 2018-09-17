#ifndef _TIGERA_INCLUDES__H__
#define _TIGERA_INCLUDES__H__

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define countof(a) (sizeof(a)/sizeof(a[0]))

#define downcast(p, type, member)  ((p) ? ((type *)(((char *)(p)) - offsetof(type, member))) : NULL)

#include "list.h"
#include "slist.h"

#endif /* _TIGERA_INCLUDES__H__ */
