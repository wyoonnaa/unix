#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <unistd.h>
#endif

#define MAX_CLIENTS 1000
#define BUFFER_SIZE 4096
#define LOG_FILE "server.log"

typedef struct {
    int fd;
    char read_buffer[BUFFER_SIZE];
    int read_buffer_len;
    char write_buffer[BUFFER_SIZE];
    int write_buffer_len;
    int write_pos;
    int active;
    long first_request_ms;
    long last_response_ms;
} Client;

Client clients[MAX_CLIENTS];
long long state = 0;
FILE *log_file;
int server_fd;

void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_file, "[%s] ", time_str);
    vfprintf(log_file, format, args);
    fflush(log_file);
    va_end(args);
}

void log_memory_info(int client_fd) {
#ifdef __APPLE__
    struct mstats stats = mstats();
    log_message("подключение клиента FD=%d, использовано байт в куче=%lu\n", client_fd, stats.bytes_used);
#else
    void *heap_break = sbrk(0);
    log_message("подключение клиента FD=%d, граница кучи=%p\n", client_fd, heap_break);
#endif
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int create_server_socket(const char *socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    
    unlink(socket_path);
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (listen(fd, MAX_CLIENTS) < 0) return -1;
    
    set_nonblocking(fd);
    return fd;
}

void flush_write_buffer(Client *client) {
    if (client->write_buffer_len == 0) return;
    
    int to_send = client->write_buffer_len - client->write_pos;
    int n = send(client->fd, client->write_buffer + client->write_pos, to_send, MSG_DONTWAIT);
    
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_message("не удалось отправить данные для FD=%d: %s\n", client->fd, strerror(errno));
            client->active = 0;
            close(client->fd);
        }
        return;
    }
    
    client->write_pos += n;
    if (client->write_pos >= client->write_buffer_len) {
        client->write_buffer_len = 0;
        client->write_pos = 0;
    }
}

void queue_response(Client *client, const char *response) {
    int resp_len = strlen(response);
    
    if (client->write_buffer_len + resp_len >= BUFFER_SIZE) {
        flush_write_buffer(client);
    }
    
    if (client->write_buffer_len + resp_len < BUFFER_SIZE) {
        memcpy(client->write_buffer + client->write_buffer_len, response, resp_len);
        client->write_buffer_len += resp_len;
    } else {
        log_message("буфер записи заполнен для FD=%d\n", client->fd);
    }
}

void process_client_data(Client *client) {
    char *line_start = client->read_buffer;
    char *line_end;
    int processed = 0;
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        
        long num = atol(line_start);
        state += num;
        
        char response[32];
        snprintf(response, sizeof(response), "%lld\n", state);
        
        queue_response(client, response);
        
        if (client->first_request_ms == 0) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            client->first_request_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            log_message("первый запрос от FD=%d, время=%ld\n", client->fd, client->first_request_ms);
        }
        
        struct timeval tv;
        gettimeofday(&tv, NULL);
        client->last_response_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        
        log_message("FD=%d, получено='%s', ответ=%lld, состояние=%lld, время=%ld\n", 
                    client->fd, line_start, state, state, client->last_response_ms);
        
        line_start = line_end + 1;
        processed = 1;
    }
    
    if (processed || line_start > client->read_buffer) {
        int remaining_len = client->read_buffer_len - (int)(line_start - client->read_buffer);
        if (remaining_len > 0) {
            memmove(client->read_buffer, line_start, remaining_len);
        }
        client->read_buffer_len = remaining_len;
        client->read_buffer[remaining_len] = '\0';
    }
}

int main() {
    const char *socket_path = "/tmp/brownian_bot.sock";
    
    log_file = fopen(LOG_FILE, "w");
    if (!log_file) return 1;
    
    signal(SIGPIPE, SIG_IGN);
    
    server_fd = create_server_socket(socket_path);
    if (server_fd < 0) {
        perror("сервер");
        return 1;
    }
    
    log_message("запуск серверс сокет=%s, pid=%d\n", socket_path, getpid());
    log_message("нач сост %lld\n", state);
    
    memset(clients, 0, sizeof(clients));
    fd_set read_fds, write_fds;
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
                
                if (clients[i].write_buffer_len > 0) {
                    FD_SET(clients[i].fd, &write_fds);
                }
            }
        }
        
        if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        if (FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0) {
                set_nonblocking(client_fd);
                int found = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!clients[i].active) {
                        clients[i].fd = client_fd;
                        clients[i].active = 1;
                        clients[i].read_buffer_len = 0;
                        clients[i].write_buffer_len = 0;
                        clients[i].write_pos = 0;
                        clients[i].first_request_ms = 0;
                        clients[i].last_response_ms = 0;
                        memset(clients[i].read_buffer, 0, BUFFER_SIZE);
                        memset(clients[i].write_buffer, 0, BUFFER_SIZE);
                        log_message("новый клиент FD=%d\n", client_fd);
                        log_memory_info(client_fd);
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    log_message("достигнуто максимальное количество клиентов, отклоняем FD=%d\n", client_fd);
                    close(client_fd);
                }
            }
        }
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && FD_ISSET(clients[i].fd, &read_fds)) {
                char buf[4096];
                int n = read(clients[i].fd, buf, sizeof(buf) - 1);
                
                if (n <= 0) {
                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        continue;
                    }
                    log_message("отключаем клиента FD=%d\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].active = 0;
                    continue;
                }
                
                buf[n] = '\0';
                
                int remaining = BUFFER_SIZE - clients[i].read_buffer_len - 1;
                int to_copy = n < remaining ? n : remaining;
                memcpy(clients[i].read_buffer + clients[i].read_buffer_len, buf, to_copy);
                clients[i].read_buffer_len += to_copy;
                clients[i].read_buffer[clients[i].read_buffer_len] = '\0';
                
                process_client_data(&clients[i]);
            }
            
            if (clients[i].active && clients[i].write_buffer_len > 0 &&
                FD_ISSET(clients[i].fd, &write_fds)) {
                flush_write_buffer(&clients[i]);
            }
        }
    }
    
    close(server_fd);
    unlink(socket_path);
    fclose(log_file);
    return 0;
}
