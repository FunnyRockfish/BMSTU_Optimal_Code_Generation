# Название плагина и файлов
PLUGIN      = my-plugin
PLUGIN_SRC  = src/lab1_plugin/lab1_plugin.cpp
PLUGIN_SO   = $(PLUGIN).so
TEST_SRC    = src/test/test.c
TEST_BIN    = test

# Компиляторы и флаги
CXX         = g++
CC          = gcc

# Флаги для компиляции плагина
# -I$(gcc -print-file-name=plugin)/include – путь к заголовочным файлам плагинов GCC
CXXFLAGS    = -g -Wall -Wextra -std=c++14 -I`gcc -print-file-name=plugin`/include -fPIC -fno-rtti
LDFLAGS     = -shared

# Цель по умолчанию: собираем плагин и тестовую программу
all: $(PLUGIN_SO) $(TEST_BIN)

# Компиляция плагина
$(PLUGIN_SO): $(PLUGIN_SRC)
	@mkdir -p plugin
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(PLUGIN_SRC) -o plugin/$(PLUGIN_SO)
	@echo "Плагин собран: plugin/$(PLUGIN_SO)"

# Компиляция тестовой программы с подключением плагина
$(TEST_BIN): $(TEST_SRC) $(PLUGIN_SO)
	$(CC) -O0 -fplugin=plugin/$(PLUGIN_SO) $(TEST_SRC) -o $(TEST_BIN)
	@echo "Тестовая программа собрана: $(TEST_BIN)"

# Очистка артефактов сборки
clean:
	@rm -rf plugin $(TEST_BIN)
	@echo "Сборка очищена"

.PHONY: all clean
