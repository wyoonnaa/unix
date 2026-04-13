#!/bin/bash


rm -f fileA fileB fileC fileD *.gz result.txt

echo -e "\n компиляция"
make clean
make

if [ ! -f myprogram ]; then
    echo "не скомпилировалось("
    exit 1
fi

echo -e "\n Создание файла A..."
chmod +x create_test_file.sh
./create_test_file.sh

echo -e "\n копир A -> B (блок 4096)..."
./myprogram fileA fileB

echo -e "\n сжатие."
gzip -c fileA > fileA.gz
gzip -c fileB > fileB.gz

echo -e "\n распаковка B.gz -> C..."
gzip -cd fileB.gz | ./myprogram fileC

echo -e "\n копирование A -> D (блок 100)..."
./myprogram -b 100 fileA fileD

echo -e "\n размеры файлов и блоков:" | tee result.txt
echo "======" >> result.txt

for file in fileA fileA.gz fileB fileB.gz fileC fileD; do
    if [ -f "$file" ]; then
        size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
        blocks=$(stat -f%b "$file" 2>/dev/null || stat -c%b "$file" 2>/dev/null)
        echo "$file: $size байт, $blocks блоков" | tee -a result.txt
    fi
done

echo -e "\n проверка идентичности:" | tee -a result.txt

if cmp -s fileA fileB; then
    echo "✓ A и B идентичны" | tee -a result.txt
else
    echo "✗ A и B  НЕ идентичны" | tee -a result.txt
fi

if cmp -s fileA fileC; then
    echo "✓ A и C идентичны" | tee -a result.txt
else
    echo "✗ A и C Не идентичны" | tee -a result.txt
fi

if cmp -s fileA fileD; then
    echo "✓ A и D идентичны" | tee -a result.txt
else
    echo "✗ A и D не идентичны" | tee -a result.txt
fi

echo -e "\n детальная информация stat:" | tee -a result.txt
echo "=>" >> result.txt
stat -f "Файл: %N, Размер: %z байт, Реальное место: %b блоков" fileA fileA.gz fileB fileB.gz fileC fileD 2>/dev/null || stat -c "Файл: %n, Размер: %s байт, Реальное место: %b блоков" fileA fileA.gz fileB fileB.gz fileC fileD 2>/dev/null | tee -a result.txt

echo -e "\n тесты все! результат тут result.txt"
