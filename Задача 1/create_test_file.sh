#!/bin/bash

FILE_A="fileA"
RAZMER=$((4 * 1024 * 1024 + 1))

echo "Создание тестового файла $FILE_A размером $RAZMER байт..."

dd if=/dev/zero of="$FILE_A" bs=1 count=$RAZMER 2>/dev/null
printf '\x01' | dd of="$FILE_A" bs=1 seek=0 count=1 conv=notrunc 2>/dev/null
printf '\x01' | dd of="$FILE_A" bs=1 seek=10000 count=1 conv=notrunc 2>/dev/null
printf '\x01' | dd of="$FILE_A" bs=1 seek=$((RAZMER - 1)) count=1 conv=notrunc 2>/dev/null

echo "Файл $FILE_A создан"
ls -lh "$FILE_A"
