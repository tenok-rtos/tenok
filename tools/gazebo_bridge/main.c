#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include "serial.h"

pthread_t thread1, thread2;
sem_t sem_terminate;

int serial_fd, socket_fd;

void *thread_gazebo_to_hil(void *arg)
{
    /* SIL/HIL to gazebo forwarding */
    char c;
    while (1) {
        c = serial_getc(serial_fd);
        write(socket_fd, &c, 1);
    }
}

void *thread_hil_to_gazebo(void *arg)
{
    /* Gazebo to SIL/HIL forwarding */
    char c;
    while (1) {
        read(socket_fd, &c, 1);
        serial_puts(serial_fd, &c, 1);
    }
}

void sig_handler(int signum)
{
    pthread_cancel(thread1);
    pthread_cancel(thread2);

    close(serial_fd);
    close(socket_fd);

    sem_post(&sem_terminate);
}

static void usage(const char *execpath)
{
    printf(
        "Usage: %s -i gazebo-ip -p port-number "
        "-s serial-name [-b baudrate]\n",
        execpath);
}

static bool parse_long_from_str(char *str, long *value)
{
    char *end_ptr = NULL;
    errno = 0;
    *value = strtol(str, &end_ptr, 10);
    if (errno != 0 || *end_ptr != '\0') {
        return false;
    } else {
        return true;
    }
}

static void handle_options(int argc,
                           char **argv,
                           char **gazebo_ip,
                           char **port_number,
                           char **serial_name,
                           char **baudrate)
{
    *gazebo_ip = *port_number = *serial_name = *baudrate = NULL;

    int optidx = 0;

    /* clang-format off */
    struct option opts[] = {
        {"ip", 1, NULL, 'i'},
        {"port", 1, NULL, 'p'},
        {"serial", 1, NULL, 's'},
        {"baudrate", 1, NULL, 'b'},
        {"help", 0, NULL, 'h'},
    };
    /* clang-format on */

    int c;
    while ((c = getopt_long(argc, argv, "i:p:s:b:h", opts, &optidx)) != -1) {
        switch (c) {
        case 'i':
            *gazebo_ip = optarg;
            break;
        case 'p':
            *port_number = optarg;
            break;
        case 's':
            *serial_name = optarg;
            break;
        case 'b':
            *baudrate = optarg;
            break;
        case 'h':
            usage(argv[0]);
            exit(1);
        default:
            break;
        }
    }

    if (!*gazebo_ip) {
        printf("Gazebo IP must be provided via -i option.\n");
        usage(argv[0]);
        exit(1);
    }

    if (!*port_number) {
        printf("Gazebo port must be provided via -p option.\n");
        usage(argv[0]);
        exit(1);
    }

    if (!*serial_name) {
        printf("Serial name must be provided via -s option.\n");
        usage(argv[0]);
        exit(1);
    }

    if (!*baudrate)
        *baudrate = "115200";

    /* Check if port number and baudrate value are valid integers */
    long val;
    if (!parse_long_from_str(*port_number, &val)) {
        printf("Bad port number.\n");
        exit(1);
    }

    if (!parse_long_from_str(*baudrate, &val)) {
        printf("Bad serial baudrate value.\n");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    /* Parse input arguments */
    char *gazebo_ip;
    char *port_number;
    char *serial_name;
    char *baudrate;

    handle_options(argc, argv, &gazebo_ip, &port_number, &serial_name,
                   &baudrate);

    printf("Gazebo IP: %s, port: %d\nSerial: %s, baudrate: %d\n", gazebo_ip,
           atoi(port_number), serial_name, atoi(baudrate));

    /* sSerial port initialization */
    serial_fd = serial_init(serial_name, atoi(baudrate));
    if (serial_fd == -1) {
        printf("Failed to connect to the serial.\n");
        exit(1);
    }

    /* Socket initialization */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        printf("Failed to create socket.\n");
        exit(1);
    }

    struct sockaddr_in serv_addr = {.sin_family = AF_INET,
                                    .sin_addr.s_addr = inet_addr(gazebo_ip),
                                    .sin_port = htons(atoi(port_number))};

    /* Attempt to connect to the Gazebo server */
    if (connect(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) {
        printf("Failed to connect to the Gazeobo.\n");
        exit(1);
    }

    printf("Established connection between SIL/HIL and Gazebo.\n");

    /* Launch threads to handle message forwarding */
    pthread_create(&thread1, NULL, thread_gazebo_to_hil, NULL);
    pthread_create(&thread2, NULL, thread_hil_to_gazebo, NULL);

    /* Install the signal handlers */
    signal(SIGINT, sig_handler);
    signal(SIGABRT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Wait until the program is terminated */
    sem_init(&sem_terminate, 0, 0);
    sem_wait(&sem_terminate);

    return 0;
}
