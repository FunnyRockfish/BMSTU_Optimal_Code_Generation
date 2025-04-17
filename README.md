```bash
# Сборка плагина
g++ -fPIC -shared -std=c++11 \
-fno-rtti \
src/lab1_plugin/lab1_plugin.cpp -o gimple_json_plugin.so \
-I$(gcc -print-file-name=plugin)/include \
-I$(gcc -print-file-name=include) \
-I$(gcc -print-file-name=plugin)/include/c-family

# Запуск плагина на C-файле
gcc src/test/test.c -O1 -fplugin=./gimple_json_plugin.so > output.json
```
