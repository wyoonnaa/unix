rm -f stats.txt testfile.lck testfile
make -f makefile.m clean 2>/dev/null
make -f makefile.m

if [ ! -f file_lock ]; then
    echo "Ошибка компиляции :(" | tee result.txt
    exit 1
fi

touch testfile
echo "запускаем 10 процессов..."
pids=()

for i in {1..10}; do
    ./file_lock testfile &
    pids+=($!)
    sleep 0.05
done

echo "5 минут... это много или мало..."
sleep 300

echo "Отправляем sigint..."
for pid in "${pids[@]}"; do
    kill -INT $pid 2>/dev/null
done

echo "ждем-с"
for pid in "${pids[@]}"; do
    wait $pid 2>/dev/null
done

cat > result.txt << EOF

EOF

if [ -f stats.txt ]; then

    cat stats.txt >> result.txt

    LINES=$(wc -l < stats.txt | tr -d ' ')
    
    ZERO_COUNT=$(grep -c ": 0$" stats.txt 2>/dev/null | head -1 | tr -d ' \n')
    if [ -z "$ZERO_COUNT" ]; then ZERO_COUNT=0; fi

    MIN=$(awk -F': ' '{print $2}' stats.txt | sort -n | head -1)
    MAX=$(awk -F': ' '{print $2}' stats.txt | sort -n | tail -1)
    
    echo "Строк в stats: $LINES " >> result.txt
    echo "Процессов с 0 блокировок: $ZERO_COUNT " >> result.txt
    echo "Блокировок: от $MIN до $MAX" >> result.txt
else
    echo "stats не создан" >> result.txt
fi

if [ -f testfile.lck ]; then
    echo "Файл блокировки НЕ удален " >> result.txt
else
    echo "Файл блокировки удален " >> result.txt
fi

echo "" >> result.txt
if [ -f stats.txt ] && [ "$ZERO_COUNT" -eq 0 ] && [ ! -f testfile.lck ]; then
    echo "ИТОГ:  ок " >> result.txt
else
    echo "ИТОГ: не ок " >> result.txt
fi


cat stats.txt

echo "Строк: $(wc -l < stats.txt | tr -d ' ') (OK)"
echo "Статистика: от $MIN до $MAX блокировок"
echo "Файл блокировки удалён"
echo "Отчёт сохранён в result.txt"
