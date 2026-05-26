#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    char *config_file = "config";
    char *input_file = NULL;
    char *output_file = NULL;
    int verify_only = 0;
    int opt;
    
    while ((opt = getopt(argc, argv, "c:i:o:v")) != -1) {
        switch (opt) {
            case 'c': config_file = optarg; break;
            case 'i': input_file = optarg; break;
            case 'o': output_file = optarg; break;
            case 'v': verify_only = 1; break;
            default:
                fprintf(stderr, "использование: %s [-c конфиг] [-i входной] [-o выходной] [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    FILE *config = fopen(config_file, "r");
    if (!config) {
        perror("fopen конфиг");
        exit(EXIT_FAILURE);
    }
    
    char socket_path[256];
    if (!fgets(socket_path, sizeof(socket_path), config)) {
        fprintf(stderr, "не удалось прочитать путь к сокету\n");
        fclose(config);
        exit(EXIT_FAILURE);
    }
    socket_path[strcspn(socket_path, "\n")] = 0;
    fclose(config);
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("сокет");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("подключение");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    if (verify_only) {
        send(sock, "0\n", 2, 0);
        char response[BUFFER_SIZE];
        int n = recv(sock, response, sizeof(response) - 1, 0);
        if (n > 0) {
            response[n] = '\0';
            printf("%s", response);
        }
        close(sock);
        return 0;
    }
    
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (input_file) {
        FILE *input = fopen(input_file, "r");
        if (!input) {
            perror("fopen входной");
            close(sock);
            exit(EXIT_FAILURE);
        }
        
        char line[12];
        char response[BUFFER_SIZE];
        
        while (fgets(line, sizeof(line), input)) {
            send(sock, line, strlen(line), 0);
            
            int n = recv(sock, response, sizeof(response) - 1, 0);
            if (n > 0) {
                response[n] = '\0';
                if (!output_file) {
                    printf("%s", response);
                }
            }
        }
        fclose(input);
    } else {
        char line[12];
        char response[BUFFER_SIZE];
        
        while (fgets(line, sizeof(line), stdin)) {
            send(sock, line, strlen(line), 0);
            
            int n = recv(sock, response, sizeof(response) - 1, 0);
            if (n > 0) {
                response[n] = '\0';
                printf("%s", response);
            }
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                      (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
    
    if (output_file) {
        FILE *out = fopen(output_file, "w");
        if (out) {
            fprintf(out, "прошло мс=%ld\n", elapsed_ms);
            fclose(out);
        }
    }
    
    close(sock);
    return 0;
}
