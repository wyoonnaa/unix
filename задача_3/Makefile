CC = gcc
CFLAGS = -Wall -g
TARGET = myinit
SOURCES = myinit.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)
	rm -f /tmp/myinit.log

run: $(TARGET)
	./runme.sh

start: $(TARGET)
	./$(TARGET) -c /tmp/test_myinit/config3.txt

stop:
	@pkill myinit 2>/dev/null || echo "myinit не запущен"

status:
	@echo "Процесс myinit"
	@pgrep myinit && echo "Запущен: PID=$$(pgrep myinit)" || echo "Не запущен"
	@echo ""
	@echo "Дочерние процессы"
	@ps aux | grep test_prog | grep -v grep || echo "Нет дочерних процессов"
	@echo ""
	@echo "Последние 5 строк лога"
	@tail -5 /tmp/myinit.log 2>/dev/null || echo "Нет лог-файла"

log:
	@cat /tmp/myinit.log 2>/dev/null || echo "Нет лог-файла"

.PHONY: all clean run start stop status log
