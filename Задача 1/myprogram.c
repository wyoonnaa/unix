// Тема: работа с файлами - open, read, write, seek.

// Sparse file — файл, в котором последовательности нулевых байтов заменены на информацию о них. Последовательность нулевых байт внутри файла (дыры) не записывается на диск, а информация о них (смещение от начала файла в байтах и количество байт) хранится в метаданных ФС. Сжатие архиваторами типа gzip происходит очень эффективно. При распаковке всё наоборот: gzip не заботится о создании дырок и забивает их нулями с риском выйти за пределы файловой системы.

// Требуется написать программу создания sparse файлов. Программа считает нули и заменяет блоки, заполненные нулями, на seek для создания разреженного файла. Поскольку запись в файл идёт поблочно, то и входные данные надо обрабатывать поблочно (бессмысленно записать 1 байт, потом сделать seek на 315 байт и записать ещё один байт - все уйдет в один блок). По умолчанию сделать размер блока 4096 байт, но отдельный параметр должен задавать размер блока в байтах.

// Если на вход программе подаётся один аргумент — имя файла, то читается stdin и пишется в указанный файл. Если два аргумента, то читается первый файл и пишется в последний.

// Также написать вспомогательный скрипт, создающий тестовый файл A, длиной 4*1024*1024 + 1 байт, заполненный в основном нулями. По смещениям 0, 10000 и в конце файла должны быть записаны единицы.

// Далее в runme.sh прописываются следующие действия:

// Создать файл A.
// Скопировать созданный файл A через нашу программу в файл B, сделав его разреженным: $ ./myprogram fileA fileB
// Сжать A и B с помощью gzip
// Распаковать сжатый файл B в stdout и сохранить через программу в файл C: $ gzip -cd fileB.gz | ./myprogram fileC
// Скопировать A через программу в файл D, указав нестандартный размер блока - 100 байт.
// Программой stat вывести реальный размер файлов A, A.gz, B, B.gz, C, D
// Ключевые функции: read(), write(), lseek(output, count, SEEK_CUR), ftruncate(output, size).


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

char *name_programm = NULL;

void error_sistem(char *mess) {
    fprintf(stderr, "%s: %s: %s\n", name_programm, mess, strerror(errno));
    exit(1);
}

int main(int argc, char *argv[]) {
    name_programm = argv[0];
    
    char *in_fail = NULL;
    char *out_fail = NULL;
    int razmer_bloka = 4096;
    int sparse_mode = 0;
    
    int opt;
    while ((opt = getopt(argc, argv, "b:")) != -1) {
        if (opt == 'b') {
            razmer_bloka = atoi(optarg);
            if (razmer_bloka <= 0) {
                fprintf(stderr, "Размер блока должен быть > 0\n");
                exit(1);
            }
        }
    }
    
    int ostatok = argc - optind;
    if (ostatok == 1) {
        out_fail = argv[optind];
        sparse_mode = 0;
    } else if (ostatok == 2) {
        in_fail = argv[optind];
        out_fail = argv[optind + 1];
        sparse_mode = 1;
    } else {
        fprintf(stderr, "Использование: %s [-b blok] [vhod] выход\n", argv[0]);
        exit(1);
    }
    

    int vhod = STDIN_FILENO;
    int nado_zakryt_vhod = 0;
    if (in_fail != NULL) {
        vhod = open(in_fail, O_RDONLY);
        if (vhod == -1) error_sistem("где файл? нет его");
        nado_zakryt_vhod = 1;
    }

    int vihod = open(out_fail, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (vihod == -1) error_sistem("не получилось создавть выходной файлик");
    
    char *bufer = malloc(razmer_bloka);
    if (bufer == NULL) {
        fprintf(stderr, "память не выделилась\n");
        exit(1);
    }
    
    long long vsego_bait = 0;
    long long null_count = 0;
    
    if (sparse_mode) {
        printf("Режим: создание разреженного файла\n");
        
        while (1) {
            int prochitano = read(vhod, bufer, razmer_bloka);
            if (prochitano == -1) error_sistem("ошибка чтения");
            if (prochitano == 0) break;
            
            int vse_nuli = 1;
            for (int i = 0; i < prochitano; i++) {
                if (bufer[i] != 0) {
                    vse_nuli = 0;
                    break;
                }
            }
            
            if (vse_nuli) {
                if (lseek(vihod, prochitano, SEEK_CUR) == -1) {
                    error_sistem("ошибка seek");
                }
                null_count += prochitano;
            } else {
                int zapisano = 0;
                while (zapisano < prochitano) {
                    int result = write(vihod, bufer + zapisano, prochitano - zapisano);
                    if (result == -1) error_sistem("ошибка записи");
                    zapisano += result;
                }
            }
            vsego_bait += prochitano;
        }
        
        if (ftruncate(vihod, vsego_bait) == -1) {
            error_sistem("ошибка установки размера");
        }
        
        printf("Создан разреженный файл размером %lld байт\n", vsego_bait);
        printf("Пропущено нулевых байт: %lld\n", null_count);
        
    } else {
        printf("Режим: обычное копирование\n");
        
        while (1) {
            int prochitano = read(vhod, bufer, razmer_bloka);
            if (prochitano == -1) error_sistem("ошибка чтения");
            if (prochitano == 0) break;
            
            int zapisano = 0;
            while (zapisano < prochitano) {
                int result = write(vihod, bufer + zapisano, prochitano - zapisano);
                if (result == -1) error_sistem("ошибка записи");
                zapisano += result;
            }
            vsego_bait += prochitano;
        }
        
        printf("Создан обычный файл размером %lld байт\n", vsego_bait);
    }
    
    free(bufer);
    close(vihod);
    if (nado_zakryt_vhod) close(vhod);
    
    return 0;
}
