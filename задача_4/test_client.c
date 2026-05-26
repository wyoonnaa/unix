#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    char *config_file = "config";
    char *input_file = NULL;
    int max_delay_ms = 0;
    char *output_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i+1 < argc) config_file = argv[++i];
        if (strcmp(argv[i], "-i") == 0 && i+1 < argc) input_file = argv[++i];
        if (strcmp(argv[i], "-d") == 0 && i+1 < argc) max_delay_ms = atoi(argv[++i]);
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) output_file = argv[++i];
    }
    
    if (!input_file) {
        fprintf(stderr, "Использование: %s -i входной_файл [-d макс_задержка_мс] [-o выходной_файл] [-c конфиг]\n", argv[0]);
        return 1;
    }
    
    srand(time(NULL) ^ getpid());
    
    FILE *config = fopen(config_file, "r");
    if (!config) return 1;
    
    char socket_path[256];
    if (!fgets(socket_path, sizeof(socket_path), config)) {
        fclose(config);
        return 1;
    }
    socket_path[strcspn(socket_path, "\n")] = 0;
    fclose(config);
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return 1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 1;
    }
    
    FILE *input = fopen(input_file, "r");
    if (!input) {
        close(sock);
        return 1;
    }
    
    char line[100];
    long total_delay_ms = 0;
    int line_count = 0;
    struct timespec start_time, end_time;
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    while (fgets(line, sizeof(line), input)) {
        line_count++;
        int len = strlen(line);
        int pos = 0;
        
        while (pos < len) {
            int chunk = (rand() % 255) + 1;
            if (chunk > len - pos) chunk = len - pos;
            
            int sent = send(sock, line + pos, chunk, 0);
            if (sent < 0) {
                perror("отправка");
                break;
            }
            pos += sent;
            
            if (pos < len && max_delay_ms > 0) {
                int sleep_ms = rand() % (max_delay_ms + 1);
                if (sleep_ms > 0) {
                    usleep(sleep_ms * 1000);
                    total_delay_ms += sleep_ms;
                }
            }
        }
    
        char response[4096];
        int total_recv = 0;
        while (total_recv < 1) {
            int n = recv(sock, response + total_recv, sizeof(response) - total_recv - 1, 0);
            if (n <= 0) break;
            total_recv += n;
            if (strchr(response, '\n')) break;
        }
        if (total_recv <= 0) break;
        response[total_recv] = '\0';
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                      (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
    
    fclose(input);
    
    send(sock, "0\n", 2, 0);
    char verify_response[64];
    int n = recv(sock, verify_response, sizeof(verify_response) - 1, 0);
    if (n > 0) {
        verify_response[n] = '\0';
    }
    
    close(sock);
    
    if (output_file) {
        FILE *out = fopen(output_file, "w");
        if (out) {
            fprintf(out, "общая_задержка_мс=%ld\n", total_delay_ms);
            fprintf(out, "прошло_мс=%ld\n", elapsed_ms);
            fprintf(out, "количество_строк=%d\n", line_count);
            fclose(out);
        }
    }
    
    return 0;
}
