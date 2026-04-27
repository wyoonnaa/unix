#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <getopt.h>

#define FILE_STAT "stats.txt"

int schetchik = 0;           
int rabotaet = 1;     
int zablokirovano = 0;       
char imya_loka[256];       
char imya_faila[256];        

void save_stat() {
    FILE *f = fopen(FILE_STAT, "a");
    if (f == NULL) {
        perror("ошибка открытия stats");
        return;
    }
    fprintf(f, "PID %d: %d блокировок\n", getpid(), schetchik);
    fclose(f);
}

void obrabotchik_sigint(int sig) {
    (void)sig; 
    rabotaet = 0;
}

int sozdat_lock() {
    int fd = open(imya_loka, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd == -1) {
        return 0;
    }
    
    char pid_stroka[32];
    int len = snprintf(pid_stroka, sizeof(pid_stroka), "%d\n", getpid());
    if (len < 0 || len >= (int)sizeof(pid_stroka)) {
        close(fd);
        return 0;
    }
    
    if (write(fd, pid_stroka, strlen(pid_stroka)) == -1) {
        perror("ошибка записи pid в лок");
        close(fd);
        return 0;
    }
    
    if (close(fd) == -1) {
        perror("ошибка закрытия лока");
        return 0;
    }
    
    zablokirovano = 1;
    return 1;
}

int proverit_lock() {
    FILE *f = fopen(imya_loka, "r");
    if (f == NULL) return 0;
    
    int pid_v_fayle;
    if (fscanf(f, "%d", &pid_v_fayle) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    
    return (pid_v_fayle == getpid());
}

void udalit_lock() {
    if (zablokirovano && proverit_lock()) {
        if (unlink(imya_loka) == -1) {
            perror("ошибка удаления лока");
        }
        zablokirovano = 0;
    }
}

int main(int argc, char *argv[]) {
    int opt;
    char *filename = NULL;
    
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                printf("Использование: %s <имя_файла>\n", argv[0]);
                exit(0);
            default:
                fprintf(stderr, "использовать: %s <имя_файла>\n", argv[0]);
                exit(1);
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, " не указано имя файла\n");
        fprintf(stderr, "использовать: %s <имя_файла>\n", argv[0]);
        exit(1);
    }
    
    filename = argv[optind];
    
    snprintf(imya_faila, sizeof(imya_faila), "%s", filename);
    snprintf(imya_loka, sizeof(imya_loka), "%s.lck", filename);
    
    signal(SIGINT, obrabotchik_sigint);
    srand(time(NULL) ^ (getpid() << 16));
    
    int zaderzhka_start = rand() % 500000;
    printf("PID %d: стартовая задержка %d ms\n", getpid(), zaderzhka_start / 1000);
    usleep(zaderzhka_start);
    
    printf("PID %d: начал работу\n", getpid());
    
    while (rabotaet) {
        // int popytki = 0;
        while (!sozdat_lock() && rabotaet) {
            int zaderzhka = (rand() % 30000) + 10000;
            usleep(zaderzhka);
            // popytki++;
        }
        
        if (!rabotaet) break;
        
        schetchik++;
        printf("PID %d: захватил (#%d)\n", getpid(), schetchik);
        
        int rab_cycle = 0;
        while (rab_cycle < 10 && rabotaet) {
            usleep(100000);
            rab_cycle++;
        }
        
        if (proverit_lock()) {
            udalit_lock();
            printf("PID %d: снял\n", getpid());
        } else {
            fprintf(stderr, "PID %d: потерял лок\n", getpid());
            break;
        }
        
        int pauza = (rand() % 50000) + 20000;
        usleep(pauza);
    }
    
    if (zablokirovano) {
        printf("PID %d: снимаем лок на выходе\n", getpid());
        udalit_lock();
    }
    
    save_stat();
    printf("PID %d: завершается, кол-во успешных блокировок: %d\n", getpid(), schetchik);
    return 0;
}
