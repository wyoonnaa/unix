#!/bin/bash

echo " Запуск тестов  "

CURRENT_DIR=$(pwd)

rm -f $CURRENT_DIR/result.txt
rm -f /tmp/myinit.log


mkdir -p /tmp/test_myinit
cd /tmp/test_myinit

cat > test_prog1.c << 'EOF'
#include <stdio.h>
#include <unistd.h>
int main() {
    int count = 0;
    while(1) {
        printf("Тестовая программа 1 запущена, count: %d\n", count++);
        fflush(stdout);
        sleep(5);
    }
    return 0;
}
EOF

cat > test_prog2.c << 'EOF'
#include <stdio.h>
#include <unistd.h>
int main() {
    int count = 0;
    while(1) {
        printf("Тестовая программа 2 запущена: %d\n", count++);
        fflush(stdout);
        sleep(5);
    }
    return 0;
}
EOF

cat > test_prog3.c << 'EOF'
#include <stdio.h>
#include <unistd.h>
int main() {
    int count = 0;
    while(1) {
        printf("Тестовая программа 3 запущена, count: %d\n", count++);
        fflush(stdout);
        sleep(5);
    }
    return 0;
}
EOF

gcc -o test_prog1 test_prog1.c
gcc -o test_prog2 test_prog2.c
gcc -o test_prog3 test_prog3.c


touch stdin1 stdin2 stdin3
touch stdout1 stdout2 stdout3

cat > config3.txt << EOF
/tmp/test_myinit/test_prog1 /tmp/test_myinit/stdin1 /tmp/test_myinit/stdout1
/tmp/test_myinit/test_prog2 /tmp/test_myinit/stdin2 /tmp/test_myinit/stdout2
/tmp/test_myinit/test_prog3 /tmp/test_myinit/stdin3 /tmp/test_myinit/stdout3
EOF

cat > config1.txt << EOF
/tmp/test_myinit/test_prog1 /tmp/test_myinit/stdin1 /tmp/test_myinit/stdout1
EOF

echo "запуск myinit с config3.txt"
$CURRENT_DIR/myinit -c /tmp/test_myinit/config3.txt
sleep 2

# Тест 1
echo " " >> $CURRENT_DIR/result.txt
echo "Тест 1: Проверка запуска 3 дочерних процессов" >> $CURRENT_DIR/result.txt
echo "Ожидается: 3 процесса (test_prog1, test_prog2, test_prog3)" >> $CURRENT_DIR/result.txt
echo "" >> $CURRENT_DIR/result.txt

ps aux | grep -E "test_prog[123]" | grep -v grep >> $CURRENT_DIR/result.txt
PROC_COUNT=$(ps aux | grep -E "test_prog[123]" | grep -v grep | wc -l | tr -d ' ')

echo "" >> $CURRENT_DIR/result.txt
echo "Фактически найдено $PROC_COUNT процессов" >> $CURRENT_DIR/result.txt
if [ $PROC_COUNT -eq 3 ]; then
    echo "успешно" >> $CURRENT_DIR/result.txt
else
    echo "неуспешно Ожидалось 3, найдено $PROC_COUNT" >> $CURRENT_DIR/result.txt
fi

echo "" >> $CURRENT_DIR/result.txt

# Тест 2

echo "Тест 2: Убийство процесса номер 2 и проверка перезапуска" >> $CURRENT_DIR/result.txt
echo "Ожидается: После убийства test_prog2 он должен перезапуститься, всё ещё 3 процесса" >> $CURRENT_DIR/result.txt
echo "" >> $CURRENT_DIR/result.txt

pkill -f test_prog2
sleep 3

ps aux | grep -E "test_prog[123]" | grep -v grep >> $CURRENT_DIR/result.txt
PROC_COUNT=$(ps aux | grep -E "test_prog[123]" | grep -v grep | wc -l | tr -d ' ')

echo "" >> $CURRENT_DIR/result.txt
echo "Найдено $PROC_COUNT процессов" >> $CURRENT_DIR/result.txt
if [ $PROC_COUNT -eq 3 ]; then
    echo "успешно Процесс был перезапущен" >> $CURRENT_DIR/result.txt
else
    echo "неуспешно т.к. Ожидалось 3, найдено $PROC_COUNT" >> $CURRENT_DIR/result.txt
fi
echo "" >> $CURRENT_DIR/result.txt

# Тест 3
echo "Тест 3: Смена конфига и отправка SIGHUP" >> $CURRENT_DIR/result.txt
echo " После SIGHUP должен запуститься только 1 процесс" >> $CURRENT_DIR/result.txt
echo "" >> $CURRENT_DIR/result.txt

cp config1.txt config3.txt

MYINIT_PID=$(pgrep myinit)
if [ -n "$MYINIT_PID" ]; then
    echo "Отправка SIGHUP процессу myinit (PID: $MYINIT_PID)" >> $CURRENT_DIR/result.txt
    kill -HUP $MYINIT_PID
    sleep 3
else
    echo "ошибка! myinit не найден" >> $CURRENT_DIR/result.txt
fi

ps aux | grep -E "test_prog[123]" | grep -v grep >> $CURRENT_DIR/result.txt
PROC_COUNT=$(ps aux | grep -E "test_prog[123]" | grep -v grep | wc -l | tr -d ' ')

echo "" >> $CURRENT_DIR/result.txt
echo "Найдено $PROC_COUNT процессов" >> $CURRENT_DIR/result.txt
if [ $PROC_COUNT -eq 1 ]; then
    echo "успешно Только 1 процесс запущен" >> $CURRENT_DIR/result.txt
else
    echo "неуспешно т к  Ожидалось 1, найдено $PROC_COUNT" >> $CURRENT_DIR/result.txt
fi
echo "" >> $CURRENT_DIR/result.txt

# Тест 4
echo "Тест 4: Проверка содержимого лог-файла" >> $CURRENT_DIR/result.txt
echo "Лог содержит запуск процессов, перезапуск и обработку SIGHUP" >> $CURRENT_DIR/result.txt
echo "" >> $CURRENT_DIR/result.txt

if [ -f /tmp/myinit.log ]; then
    echo "Содержимое лог-файла:" >> $CURRENT_DIR/result.txt
    echo " " >> $CURRENT_DIR/result.txt
    cat /tmp/myinit.log >> $CURRENT_DIR/result.txt
    echo "" >> $CURRENT_DIR/result.txt
    
    if grep -q "запущен процесс 0" /tmp/myinit.log && \
       grep -q "запущен процесс 1" /tmp/myinit.log && \
       grep -q "запущен процесс 2" /tmp/myinit.log && \
       grep -q "перезапуск процесса" /tmp/myinit.log && \
       grep -q "получен SIGHUP" /tmp/myinit.log && \
       grep -q "киллим процесс" /tmp/myinit.log && \
       grep -q "Прочитано 1" /tmp/myinit.log; then
        echo "усипшно Все ожидаемые записи в логе найдены" >> $CURRENT_DIR/result.txt
    else
        echo "неуспешно  ожидаемые записи в логе отсутствуют" >> $CURRENT_DIR/result.txt
        echo "" >> $CURRENT_DIR/result.txt
        echo "Ожидаемые записи:" >> $CURRENT_DIR/result.txt
        echo " Запущен процесс 0, 1, 2" >> $CURRENT_DIR/result.txt
        echo "  Перезапуск процесса" >> $CURRENT_DIR/result.txt
        echo " Получен сигнал SIGHUP" >> $CURRENT_DIR/result.txt
        echo "  Убиваем процесс" >> $CURRENT_DIR/result.txt
        echo " Прочитано 1 процесс из конфига" >> $CURRENT_DIR/result.txt
    fi
else
    echo "неуспешно т к Лог-файл не найден по адресу /tmp/myinit.log" >> $CURRENT_DIR/result.txt
fi

echo "" >> $CURRENT_DIR/result.txt
echo "Очистка процессов" >> $CURRENT_DIR/result.txt
pkill myinit 2>/dev/null
pkill test_prog 2>/dev/null
sleep 1

echo "" >> $CURRENT_DIR/result.txt
echo " тесты завершены " >> $CURRENT_DIR/result.txt


cat $CURRENT_DIR/result.txt
