#include <errno.h>
#include <error.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "simple.h"

int g_sockfd; // CAN socket FD
bool g_should_exit = false;

/**
 * @brief iso14229.h required function
 */
uint32_t iso14229UserGetms() {
    struct timeval te;
    gettimeofday(&te, NULL);                                         // get current time
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000; // calculate milliseconds
    return milliseconds;
}

/**
 * @brief iso14229.h required function
 */
uint32_t iso14229UserSendCAN(const uint32_t arbitration_id, const uint8_t *data,
                             const uint8_t size) {
    struct can_frame frame = {0};

    frame.can_id = arbitration_id;
    frame.can_dlc = size;
    memcpy(frame.data, data, size);

    if (write(g_sockfd, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write err");
        exit(-1);
    }
    return 0;
}

/**
 * @brief simple.h required function
 */
int hostCANRxPoll(uint32_t *arb_id, uint8_t *data, uint8_t *size) {
    struct can_frame frame = {0};

    int nbytes = read(g_sockfd, &frame, sizeof(struct can_frame));

    if (nbytes < 0) {
        if (EAGAIN == errno || EWOULDBLOCK == errno) {
            return -1;
        } else {
            perror("Read err");
            exit(-1);
        }
    }

    *arb_id = frame.can_id;
    *size = frame.can_dlc;
    memcpy(data, frame.data, *size);
    return 0;
}

/**
 * @brief close file descriptor on SIGINT
 *
 * @param signum
 */
void teardown(int signum) {
    if (close(g_sockfd) < 0) {
        perror("failed to close socket");
        exit(-1);
    }
    g_should_exit = true;
}

int msleep(long tms) {
    struct timespec ts;
    int ret;

    if (tms < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;

    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

    return ret;
}

struct sigaction action;
struct sockaddr_can addr;
struct ifreq ifr;
struct stat fd_stat;
FILE *fd;

int main(int ac, char **av) {
    memset(&action, 0, sizeof(action));
    action.sa_handler = teardown;
    sigaction(SIGINT, &action, NULL);

    if ((g_sockfd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW)) < 0) {
        perror("Socket");
        return -1;
    }

    if (ac < 2) {
        printf("usage: %s [socketCAN link]\n", av[0]);
        exit(-1);
    }

    strcpy(ifr.ifr_name, av[1]);
    ioctl(g_sockfd, SIOCGIFINDEX, &ifr);

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(g_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind");
        exit(-1);
    }

    // Try sending a message. This will fail if the network is down.
    iso14229UserSendCAN(0x111, (uint8_t[4]){1, 2, 3, 4}, 4);

    printf("listening on %s\n", av[1]);

    simpleServerInit();
    while (!g_should_exit) {
        simpleServerPeriodicTask();
        msleep(10);
    }
}
