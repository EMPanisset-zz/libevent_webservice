#include "http_service.h"

void
daemonize(void)
{
    pid_t pid, sid;
        
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);       
    
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }
    
    
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

static struct sockaddr_in addr4;
static struct sockaddr_storage ss;
static int nworkers = 4;
static int background = 0;

void
usage(char **argv)
{

    fprintf(stderr, "Usage: %s [-a <ipv4>] [-p <port>] [-n <# workers>] [-d <makes process a daemon if present>]\n",
            argv[0]);
};

int
process_args(int argc, char **argv)
{
    unsigned short port;

    int opt;

    while ((opt = getopt(argc, argv, "a:p:n:d:h:?")) != -1) {
        switch (opt) {

        case 'a':
            if (1 != inet_pton(AF_INET, optarg, &addr4.sin_addr)) {
                fprintf(stderr, "Invalid ipv4 argument");
                usage(argv);
                return -1;
            }
            fprintf(stderr, "%s: ipv4 listening address set %s\n", __func__, optarg);
            break;

        case 'p':
            if (1 != sscanf(optarg, "%hu", &port)) {
                fprintf(stderr, "Invalid port argument");
                usage(argv);
                return -1;
            }
            addr4.sin_port = htons(port);
            break;

        case 'n':
            if (1 != sscanf(optarg, "%d", &nworkers)) {
                fprintf(stderr, "Invalid # workers argument");
                usage(argv);
                return -1;
            }
            break;

        case 'd':
            background = 1;
            break;

        case 'h':
        case '?':
        /* fallthrough */

        default: /* '?' */
            usage(argv);
            return -1;
        }
    }
    return 0;
}

int
main(int argc, char **argv)
{
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(5000);
    inet_pton(AF_INET, "127.0.0.1", &addr4.sin_addr);

    if (process_args(argc, argv) < 0) {
        exit(EXIT_FAILURE);
    }

    memcpy(&ss, &addr4, sizeof(struct sockaddr_in));

    if (background) {
        daemonize();
    }

    http_service_init(nworkers, &ss);
    http_service_start();
    http_service_fini();

    return 0;
}
