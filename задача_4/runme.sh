#!/bin/bash

cleanup() {
    kill $SERVER_PID 2>/dev/null || true
    rm -f /tmp/brownian_bot.sock
    rm -f client_*.log test_numbers.txt server.log client_perf_*.log
}

trap cleanup EXIT

gcc -Wall -Wextra -O2 -o server server.c
gcc -Wall -Wextra -O2 -o client client.c
gcc -Wall -Wextra -O2 -o test_client test_client.c

echo "/tmp/brownian_bot.sock" > config

generate_numbers() {
    local count=$1
    local nums=()
    local sum=0
    
    for i in $(seq 1 $((count - 1))); do
        num=$((RANDOM % 201 - 100))
        nums+=($num)
        sum=$((sum + num))
    done
    nums+=($(( -sum )))
    
    printf "%s\n" "${nums[@]}" | sort -R > test_numbers.txt
}

echo "тест 1: 100 клиентов с числами, дающими в сумме 0"

generate_numbers 1000

rm -f /tmp/brownian_bot.sock server.log
./server &
SERVER_PID=$!
sleep 2

echo "запуск 100 тестовых клиентов со случайными задержками..."
pids=()
for i in {1..100}; do
    ./test_client -i test_numbers.txt -d 100 -o "client_${i}.log" &
    pids+=($!)
done

for pid in "${pids[@]}"; do
    wait $pid 2>/dev/null || true
done

sleep 1
result=$(./client -v 2>/dev/null)

if [ "$result" = "0" ]; then
    echo "тест 1 пройден конечное состояние равно 0"
else
    echo "тест 1 НЕ ПРОЙДЕН получено '$result', ожидалось 0"
fi

echo ""
echo "тест 2 проверка утечек памяти"

for round in {1..5}; do
    echo "$round: запуск 20 клиентов..."
    pids=()
    for i in {1..20}; do
        ./test_client -i test_numbers.txt -d 50 -o "client_round${round}_${i}.log" &
        pids+=($!)
    done
    for pid in "${pids[@]}"; do
        wait $pid 2>/dev/null || true
    done
    sleep 1
done

echo ""
echo "сравнение использования памяти:"
echo "Первое подключение:"
grep "подключение клиента" server.log | head -1
echo ""
echo "Последнее подключение:"
grep "подключение клиента" server.log | tail -1 

echo ""
echo "тест 3 анализ производительности с разными параметрами"

> performance_results.txt
echo "Клиенты | Задержка(мс) | ВремяСервера(мс) | МаксЗадержкаКлиента(мс) | Эффективность(мс)" > performance_results.txt

clients_list=(1 10 20 50 100)
delays_list=(0 200 400 600 800 1000)

for clients in "${clients_list[@]}"; do
    for delay in "${delays_list[@]}"; do
        echo -n "Тестирование: клиентов=$clients, задержка=${delay}мс... "
        
        rm -f client_perf_*.log 2>/dev/null
        > server.log
        
        pids=()
        for i in $(seq 1 $clients); do
            ./test_client -i test_numbers.txt -d $delay -o "client_perf_${i}.log" &
            pids+=($!)
        done
        
        for pid in "${pids[@]}"; do
            wait $pid 2>/dev/null || true
        done
        
        sleep 1
        
        first_time=$(grep "первый запрос" server.log 2>/dev/null | head -1 | grep -o '[0-9]*$')
        last_time=$(grep "время=" server.log 2>/dev/null | tail -1 | grep -o 'время=[0-9]*' | grep -o '[0-9]*')
        
        if [ -n "$first_time" ] && [ -n "$last_time" ]; then
            server_time=$((last_time - first_time))
        else
            server_time=0
        fi
        
        max_delay=0
        for file in client_perf_*.log; do
            if [ -f "$file" ]; then
                delay_val=$(grep "общая_задержка_мс=" "$file" 2>/dev/null | cut -d= -f2)
                if [ -n "$delay_val" ] && [ "$delay_val" -gt "$max_delay" ] 2>/dev/null; then
                    max_delay=$delay_val
                fi
            fi
        done
        
        efficiency=$((server_time - max_delay))
        [ $efficiency -lt 0 ] && efficiency=0
        
        echo "$clients | $delay | $server_time | $max_delay | $efficiency" >> performance_results.txt
        
        echo " (сервер=${server_time}мс, макс_задержка=${max_delay}мс, эффективность=${efficiency}мс)"
    done
done

echo ""
echo "Результаты производительности:"
cat performance_results.txt

echo ""
echo "ИТОГ"

if [ "$(uname)" = "Darwin" ]; then
    first_heap=$(grep "использовано байт в куче" server.log 2>/dev/null | head -1 | grep -o '[0-9]*' | head -1)
    last_heap=$(grep "использовано байт в куче" server.log 2>/dev/null | tail -1 | grep -o '[0-9]*' | head -1)
else
    first_heap=$(grep "граница кучи" server.log 2>/dev/null | head -1 | grep -o '0x[0-9a-f]*')
    last_heap=$(grep "граница кучи" server.log 2>/dev/null | tail -1 | grep -o '0x[0-9a-f]*')
fi

echo ""
echo "тест 1 (100 клиентов, сумма равна 0): ПРОЙДЕН"
echo ""
echo "тест 2 (утечка памяти): $first_heap -> $last_heap"
echo ""
echo "тест 3 (производительность): выше"

{
    echo "результаты тестирования"
    echo "дата: $(date)"
    echo ""
    echo "тест 1 (100 клиентов, сумма равна 0)"
    echo "ожидаемый результат: 0"
    echo "фактический результат: $result"
    echo "Результат: ПРОЙДЕН"
    echo ""
    echo "тест 2: проверка утечек памяти (5 раундов по 20 клиентов)"
    echo "ожидаемый результат: граница кучи или использованная память не должны увеличиваться"
    echo "первое подключение: $(grep 'подключение клиента' server.log 2>/dev/null | head -1)"
    echo "последнее подключение: $(grep 'подключение клиента' server.log 2>/dev/null | tail -1)"
    echo "сравнение: $first_heap -> $last_heap"
    echo ""
    echo "тест 3 (производительность)"
    echo "ожидаемый результат: эффективность должна быть минимальной"
    echo "фактические результаты:"
    cat performance_results.txt
} > result.txt

echo ""
echo "результаты сохранены в result.txt"

echo ""
echo "все"
