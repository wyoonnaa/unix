#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>

#define MAX_LINE 1024
#define MAX_PROC 100
#define MAX_ARGS 20

typedef struct {
    char path[MAX_LINE];
    char *args[MAX_ARGS];
    char stdin_file[MAX_LINE];
    char stdout_file[MAX_LINE];
    pid_t pid;
    int active;
} proc_info_t;

proc_info_t processes[MAX_PROC];
int proc_count = 0;
int log_fd = -1;
char config_file[MAX_LINE];

void write_log(const char *message) {
    if (log_fd >= 0) {
        write(log_fd, message, strlen(message));
        write(log_fd, "\n", 1);
    }
}

int parse_config_line(char *line, proc_info_t *proc) {
    char *token;
    int arg_count = 0;
    char line_copy[MAX_LINE];
    char *saveptr;
    
    memset(proc, 0, sizeof(proc_info_t));
    line[strcspn(line, "\n")] = 0;
    
    if (strlen(line) == 0) return 0;
    
    strcpy(line_copy, line);
    
    token = strtok_r(line_copy, " ", &saveptr);
    if (!token) return 0;
    
    if (token[0] != '/') {
        char err_msg[MAX_LINE];
        snprintf(err_msg, MAX_LINE, " Путь %s не является абсолютным", token);
        write_log(err_msg);
        return 0;
    }
    
    strcpy(proc->path, token);
    proc->args[arg_count] = malloc(strlen(token) + 1);
    if (!proc->args[arg_count]) {
        write_log("Не удалось выделить память");
        return 0;
    }
    strcpy(proc->args[arg_count], token);
    arg_count++;
    
    int file_count = 0;
    while ((token = strtok_r(NULL, " ", &saveptr)) != NULL && arg_count < MAX_ARGS - 1) {
        if (file_count < 2 && (strchr(token, '/') != NULL || token[0] == '/')) {
            if (file_count == 0) {
                strcpy(proc->stdin_file, token);
                file_count++;
            } else if (file_count == 1) {
                strcpy(proc->stdout_file, token);
                file_count++;
            }
        } else {
            proc->args[arg_count] = malloc(strlen(token) + 1);
            if (!proc->args[arg_count]) {
                write_log(" Не удалось выделить память");
                return 0;
            }
            strcpy(proc->args[arg_count], token);
            arg_count++;
        }
    }
    
    proc->args[arg_count] = NULL;
    proc->active = 0;
    
    return 1;
}

int read_config() {
    FILE *fp;
    char line[MAX_LINE];
    int count = 0;
    
    fp = fopen(config_file, "r");
    if (!fp) {
        char err_msg[MAX_LINE];
        snprintf(err_msg, MAX_LINE, "Не удалось открыть конфиг: %s", config_file);
        write_log(err_msg);
        return 0;
    }
    
    while (fgets(line, MAX_LINE, fp) && count < MAX_PROC) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (parse_config_line(line, &processes[count])) {
            count++;
        }
    }
    
    fclose(fp);
    proc_count = count;
    
    char log_msg[MAX_LINE];
    snprintf(log_msg, MAX_LINE, "Прочитано %d процессов из конфига", proc_count);
    write_log(log_msg);
    
    return proc_count;
}

void start_process(int index) {
    pid_t pid;
    
    pid = fork();
    
    if (pid < 0) {
        char err_msg[MAX_LINE];
        snprintf(err_msg, MAX_LINE, "ошибка fork для процесса %d: %s", 
                 index, strerror(errno));
        write_log(err_msg);
        return;
    }
    
    if (pid == 0) {
        if (strlen(processes[index].stdin_file) > 0) {
            int fd_in = open(processes[index].stdin_file, O_RDONLY);
            if (fd_in < 0) {
                char err_msg[MAX_LINE];
                snprintf(err_msg, MAX_LINE, "не удалось открыть stdin файл: %s", 
                         processes[index].stdin_file);
                write_log(err_msg);
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        } else {
            int fd_in = open("/dev/null", O_RDONLY);
            if (fd_in < 0) {
                write_log("не удалось открыть /dev/null для stdin");
                exit(1);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        
        if (strlen(processes[index].stdout_file) > 0) {
            int fd_out = open(processes[index].stdout_file, 
                             O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd_out < 0) {
                char err_msg[MAX_LINE];
                snprintf(err_msg, MAX_LINE, "не удалось открыть stdout файл: %s", 
                         processes[index].stdout_file);
                write_log(err_msg);
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        } else {
            int fd_out = open("/dev/null", O_WRONLY);
            if (fd_out < 0) {
                write_log("не удалось открыть /dev/null для stdout");
                exit(1);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        
        execvp(processes[index].path, processes[index].args);
        
        char err_msg[MAX_LINE];
        snprintf(err_msg, MAX_LINE, "ошибка exec для %s: %s", 
                 processes[index].path, strerror(errno));
        write_log(err_msg);
        exit(1);
    }
    
    processes[index].pid = pid;
    processes[index].active = 1;
    
    char log_msg[MAX_LINE];
    snprintf(log_msg, MAX_LINE, "запущен процесс %d: %s (PID: %d)", 
             index, processes[index].path, pid);
    write_log(log_msg);
}

void start_all_processes() {
    for (int i = 0; i < proc_count; i++) {
        if (!processes[i].active) {
            start_process(i);
        }
    }
}

void kill_all_processes() {
    for (int i = 0; i < proc_count; i++) {
        if (processes[i].active) {
            char log_msg[MAX_LINE];
            snprintf(log_msg, MAX_LINE, "киллим процесс %d (PID: %d)", 
                     i, processes[i].pid);
            write_log(log_msg);
            
            if (kill(processes[i].pid, SIGTERM) < 0) {
                snprintf(log_msg, MAX_LINE, "не удалось убить процесс %d: %s", 
                         i, strerror(errno));
                write_log(log_msg);
            }
            processes[i].active = 0;
            processes[i].pid = 0;
        }
    }
}

void handle_sighup(int sig) {
    write_log("получен SIGHUP");
    
    kill_all_processes();
    
    if (read_config() == 0) {
        write_log("нет валидных процессов в конфиге");
        return;
    }

    start_all_processes();
}

void daemonize() {
    pid_t pid;
    struct rlimit flim;
    int fd;
    
    if (getppid() != 1) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
    }
    
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid > 0) {
        exit(0);
    }
    
    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }
    
    if (getrlimit(RLIMIT_NOFILE, &flim) < 0) {
        perror("getrlimit");
        exit(1);
    }
    for (fd = 0; fd < (int)flim.rlim_max; fd++) {
        close(fd);
    }
    
    if (chdir("/") != 0) {
        perror("chdir");
        exit(1);
    }

    log_fd = open("/tmp/myinit.log", O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (log_fd < 0) {
        perror("open log");
        exit(1);
    }
    
    write_log("демон myinit запущен");
}

void cleanup() {
    for (int i = 0; i < proc_count; i++) {
        for (int j = 0; processes[i].args[j] != NULL; j++) {
            free(processes[i].args[j]);
        }
    }
    if (log_fd >= 0) {
        close(log_fd);
    }
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "использ: %s -c <конфиг_файл>\n", prog_name);
    fprintf(stderr, "опции:\n");
    fprintf(stderr, "  -c <файл>  путь к конфигурационному файлу (требуется абсолютный путь)\n");
    fprintf(stderr, "  -h         показать эту справку\n");
    fprintf(stderr, "\nформат конфиг файла:\n");
    fprintf(stderr, "  <путь_к_программе> [аргументы] <stdin_файл> <stdout_файл>\n");
    fprintf(stderr, "  все пути должны быть абсолютными (начинаться с '/')\n");
}

int main(int argc, char *argv[]) {
    int opt;
    char *config_path = NULL;
    
    while ((opt = getopt(argc, argv, "c:h")) != -1) {
        switch (opt) {
            case 'c':
                config_path = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (!config_path) {
        fprintf(stderr, "Требуется конфигурационный файл\n");
        print_usage(argv[0]);
        return 1;
    }
  
    if (config_path[0] != '/') {
        fprintf(stderr, "Путь к конфигу должен быть абсолютным (начинаться с '/')\n");
        return 1;
    }
    
    strcpy(config_file, config_path);
    
    daemonize();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sighup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        write_log("не удалось установить обработчик SIGHUP");
        cleanup();
        return 1;
    }
    
    if (read_config() == 0) {
        write_log("нет валидных процессов в конфиге");
        cleanup();
        return 1;
    }

    start_all_processes();

    while (1) {
        int status;
        pid_t finished_pid = waitpid(-1, &status, 0);
        
        if (finished_pid > 0) {
            for (int i = 0; i < proc_count; i++) {
                if (processes[i].active && processes[i].pid == finished_pid) {
                    char log_msg[MAX_LINE];
                    
                    if (WIFEXITED(status)) {
                        snprintf(log_msg, MAX_LINE, 
                                "процесс %d (PID: %d) завершился с кодом %d",
                                i, finished_pid, WEXITSTATUS(status));
                    } else if (WIFSIGNALED(status)) {
                        snprintf(log_msg, MAX_LINE,
                                "процесс %d (PID: %d) убит сигналом %d",
                                i, finished_pid, WTERMSIG(status));
                    } else {
                        snprintf(log_msg, MAX_LINE,
                                "процесс %d (PID: %d) завершился", i, finished_pid);
                    }
                    write_log(log_msg);
                    
                    snprintf(log_msg, MAX_LINE, "перезапуск процесса %d", i);
                    write_log(log_msg);
                    
                    processes[i].active = 0;
                    processes[i].pid = 0;
                    start_process(i);
                    break;
                }
            }
        } else if (finished_pid < 0 && errno != EINTR) {
            char err_msg[MAX_LINE];
            snprintf(err_msg, MAX_LINE, "ошибка %s", strerror(errno));
            write_log(err_msg);
        }
    }
    
    cleanup();
    return 0;
}
