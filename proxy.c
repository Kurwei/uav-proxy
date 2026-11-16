#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <getopt.h>
#include "key.h"

#define LOCAL_APPLICATION_IP "127.0.0.1"
#define BUFFER_SIZE 2048

enum {
    OPT_TUN_IN_IP = 1000,
    OPT_TUN_OUT_IP,
    OPT_TUN_IN_PORT,
    OPT_TUN_OUT_PORT,
    OPT_LOCAL_IN_PORT,
    OPT_LOCAL_OUT_PORT
};

unsigned char rotl8(unsigned char value, unsigned int count);
unsigned char rotr8(unsigned char value, unsigned int count);
void encrypt_data(unsigned char *data, ssize_t length);
void decrypt_data(unsigned char *data, ssize_t length);
void print_options(char *argv[]);

int main(int argc, char *argv[]) {

    int opt;
    char *tun_out_ip = NULL;
    char *tun_in_ip = NULL;
    int tun_out_port = 0;
    int tun_in_port = 0;
    int local_out_port = 0;
    int local_in_port = 0;

    static struct option long_options[] = {
        {"tun-in-ip", required_argument, 0, OPT_TUN_IN_IP},
        {"tun-out-ip", required_argument, 0, OPT_TUN_OUT_IP},
        {"tun-out-port", required_argument, 0, OPT_TUN_OUT_PORT},
        {"tun-in-port", required_argument, 0, OPT_TUN_IN_PORT},
        {"local-in-port", required_argument, 0, OPT_LOCAL_IN_PORT},
        {"local-out-port", required_argument, 0, OPT_LOCAL_OUT_PORT},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1){
        switch (opt) {
        case OPT_TUN_IN_IP:
            tun_in_ip = optarg;
            break;
        case OPT_TUN_OUT_IP:
            tun_out_ip = optarg;
            break;
        case OPT_TUN_IN_PORT:
            tun_in_port = atoi(optarg);
            break;
        case OPT_TUN_OUT_PORT:
            tun_out_port = atoi(optarg);
            break;
        case OPT_LOCAL_IN_PORT:
            local_in_port = atoi(optarg);
            break;
        case OPT_LOCAL_OUT_PORT:
            local_out_port = atoi(optarg);
            break;
        case '?':
            print_options(argv);
            exit(1);
        }
    }

    if(tun_out_ip == NULL || tun_in_port == 0 || tun_out_port == 0){
        print_options(argv);
        exit(1);
    }
    if(local_in_port == 0) {
        printf("Note: Not declearing local_in_port. Program will use default port as 14551. \n");
        local_in_port = 14551;
    }
    if(local_out_port == 0) {
        printf("Note: Not declearing local_out_port. Program will use default port as 14550. \n");
        local_out_port = 14550;
    }
    
    int local_sock;
    int tunnel_sock;
    struct sockaddr_in local_in_addr, local_out_addr, tun_out_addr, tun_in_addr;
    unsigned char buffer[BUFFER_SIZE];

    if((local_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation error!");
        exit(1);
    }

    if((tunnel_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation error!");
        exit(1);
    }

    memset(&local_in_addr, 0, sizeof(local_in_addr));
    local_in_addr.sin_family = AF_INET;
    local_in_addr.sin_port = htons(local_in_port);
    local_in_addr.sin_addr.s_addr = INADDR_ANY;

    memset(&local_out_addr, 0, sizeof(local_out_addr));
    local_out_addr.sin_family = AF_INET;
    local_out_addr.sin_port = htons(local_out_port);
    if (inet_pton(AF_INET, LOCAL_APPLICATION_IP, &local_out_addr.sin_addr) <= 0) {
        perror("Invalid ip address");
        exit(1);
    }

    memset(&tun_out_addr, 0, sizeof(tun_out_addr));
    tun_out_addr.sin_family = AF_INET;
    tun_out_addr.sin_port = htons(tun_out_port);
    if (inet_pton(AF_INET, tun_out_ip, &tun_out_addr.sin_addr) <= 0) {
        perror("Invalid ip address");
        exit(1);
    }

    memset(&tun_in_addr, 0, sizeof(tun_in_addr));
    tun_in_addr.sin_family = AF_INET;
    tun_in_addr.sin_port = htons(tun_in_port);
    tun_in_addr.sin_addr.s_addr = INADDR_ANY;
    
    if(bind(local_sock, (const struct sockaddr *)&local_in_addr, sizeof(local_in_addr)) < 0) {
        perror("Binding error, port might be in use");
        close(local_sock);
        exit(1);
    }

    if(bind(tunnel_sock, (const struct sockaddr *)&tun_in_addr, sizeof(tun_in_addr)) < 0) {
        perror("Binding error, port might be in use");
        close(tunnel_sock);
        exit(1);
    }

    struct pollfd fds[2];
        fds[0].events = POLLIN;
        fds[0].fd = local_sock;

        fds[1].events = POLLIN;
        fds[1].fd = tunnel_sock;

    while(1) {
        int ready = poll(fds, 2, -1);
        if(ready < 0) {
            perror("poll");
            exit(1);
        }

        struct sockaddr_in sender_addr;
        socklen_t sender_addr_len = sizeof(sender_addr);

        if(fds[0].fd != -1 && fds[0].revents & POLLIN) {
        
            ssize_t n = recvfrom(local_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);

            if(n < 0) { perror("Packet reciving error!"); continue; }

            encrypt_data(buffer, n);

            ssize_t send_bytes = sendto(tunnel_sock, buffer, n, 0, (struct sockaddr *)&tun_out_addr, sizeof(tun_out_addr));

            if (send_bytes < 0) { perror("Packet sending error!"); }

        }
        if(fds[1].fd != -1 && fds[1].revents & POLLIN) {

            ssize_t n = recvfrom(tunnel_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender_addr, &sender_addr_len);

            if(n < 0) { perror("Packet reciving error!"); continue; }

            decrypt_data(buffer, n);

            ssize_t send_bytes = sendto(local_sock, buffer, n, 0, (struct sockaddr *)&local_out_addr, sizeof(local_out_addr));

            if (send_bytes < 0) { perror("Packet sending error!"); }
            
        }
    }

    close(local_sock);
    close(tunnel_sock);
    return(0);
}

unsigned char rotl8(unsigned char value, unsigned int count) {
    count %= 8;
    return (value << count) | (value >> (8 - count));
}

unsigned char rotr8(unsigned char value, unsigned int count) {
    count %= 8;
    return (value >> count) | (value << (8 - count));
}

void encrypt_data(unsigned char *data, ssize_t length) {
    const char *key = PSK_KEY;
    ssize_t key_len =  sizeof(PSK_KEY) - 1;

    for(ssize_t i = 0; i < length; i++) {
        unsigned char k = (unsigned char)key[(i % key_len)];
        unsigned char shift = (k % 42) * 7;

        unsigned char step = (data[i] ^ k);
        data[i] = rotl8(step, shift);
    }
}

void decrypt_data(unsigned char *data, ssize_t length) {
    const char *key = PSK_KEY;
    size_t key_len = sizeof(PSK_KEY) - 1;

    for (ssize_t i = 0; i < length; i++) {
        unsigned char k = (unsigned char)key[i % key_len];
        unsigned char shift = (k % 42) * 7;

        unsigned char step = rotr8(data[i], shift);
        data[i] = step ^ k;
    }
}

void print_options(char *argv[]) {
    printf("----------------------------------------------------------------------------------------------------------");
    printf("Usage: %s [OPTIONS] \n", argv[0]);
    printf("Setting Local Options: \n");
    printf("  --local-in-port <port>   Application listening port (Default port 14551) [NOT REQUIRED]\n");
    printf("  --local-out-port <port>   Application data sending port (Default port 14550) [NOT REQUIRED] \n\n");
    printf("Setting Tunnel Options: \n");
    printf("  --tun-in-ip <ip>   This PC's own ip address [NOT REQUIRED] \n");
    printf("  --tun-out-ip <ip>   Target's own ip address [REQUIRED] \n");
    printf("  --tun-in-port <port>   This PC's connection port to target [REQUIRED] \n");
    printf("  --tun-out-port <port>   Target's connection port to this PC [REQUIRED] \n");
    printf("----------------------------------------------------------------------------------------------------------");
}