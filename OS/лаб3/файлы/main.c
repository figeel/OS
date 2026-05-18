#define _CRT_SECURE_NO_WARNINGS

// --- Подключение библиотек ---
#include <stdio.h>      // Ввод/вывод (fopen, fscanf, fprintf, printf и др.)
#include <string.h>     // Работа с памятью: memset, memcpy и др.
#include <stdlib.h>     // Общие функции: malloc, exit и др.
#include <sys/param.h>  // Макрос MIN (минимальное из двух значений)

// --- Константы ---
#define PAGE_SIZE 256                      // Размер одной страницы в байтах
#define COUNT_MEMORY_PAGES 256             // Кол-во кадров в оперативной памяти
#define COUNT_BACKING_STORE_PAGES 256      // Кол-во страниц в backing store (вторичной памяти)
#define TLB_SIZE 16                        // Размер буфера TLB (ассоциативной памяти)
#define MEMORY_SIZE PAGE_SIZE * COUNT_MEMORY_PAGES  // Общий объём оперативной памяти
#define BACKING_STORE_SIZE PAGE_SIZE * COUNT_BACKING_STORE_PAGES // Общий объём backing store

// --- Глобальные массивы ---
char memory[MEMORY_SIZE];                 // Симуляция физической памяти (RAM)
char backing_store[BACKING_STORE_SIZE];  // Симуляция backing store (вторичной памяти)

// Таблицы:
int page_table[COUNT_BACKING_STORE_PAGES][2]; // Таблица страниц: [вирт. страница][кадр]
int tlb_table[TLB_SIZE][2];                  // Ассоциативный буфер (TLB): [вирт. страница][кадр]
int claim_table[COUNT_MEMORY_PAGES][2];      // Таблица использования кадров: [занятость][счётчик использования]

int tlb_size = 0;             // Текущий размер TLB
int page_table_size = 0;      // Текущий размер таблицы страниц

// --- Главная функция ---
int main(int argc, char* argv[]) {

    // --- Инициализация таблиц ---
    memset(page_table, -1, sizeof(int) * COUNT_BACKING_STORE_PAGES * 2); // Все страницы помечаем как отсутствующие
    memset(tlb_table, -1, sizeof(int) * TLB_SIZE * 2);                    // TLB изначально пуст
    memset(claim_table, 0, sizeof(int) * COUNT_MEMORY_PAGES * 2);        // Все кадры свободны

    // --- Открытие файлов ---
    FILE* file = fopen("backing_store.bin", "rb");     // Файл-образ вторичной памяти
    FILE* addreses = fopen("addresses.txt", "r");       // Файл с виртуальными адресами
    FILE* result = fopen("out.txt", "w");                // Файл для результатов

    // --- Счётчики статистики ---
    int tlb_hits = 0;     // Количество попаданий в TLB
    int page_faults = 0;  // Количество ошибок страниц (Page Fault)
    int total = 0;        // Общее количество обработанных адресов

    int virtual_address;                                      // Переменная для хранения виртуального адреса из файла
    while (fscanf(addreses, "%d", &virtual_address) == 1) {   // Чтение адресов из файла, пока они есть

        int page = virtual_address >> 8;                      // Выделение номера страницы (старшие 8 бит)
        int offset = virtual_address & 0x00FF;                // Выделение смещения (младшие 8 бит)

        long frame = -1;                                      // Номер кадра, соответствующий странице (изначально неизвестен)
        int value;                                            // Значение, хранимое по физическому адресу
        int tlb_size_temp = tlb_size;                         // Временная копия размера TLB
        int page_table_size_temp = page_table_size;           // Временная копия размера таблицы страниц

        // --- Поиск страницы в TLB ---
        for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++) {    // Перебор записей в TLB
            if (tlb_table[x][0] == page) {                          // Если нашли соответствующую страницу
                frame = tlb_table[x][1];                            // Получаем номер кадра
                value = memory[frame * PAGE_SIZE + offset];         // Извлекаем значение из физической памяти
                tlb_hits++;                                         // Увеличиваем счётчик попаданий в TLB
                break;                                              // Прерываем цикл
            }
        }

        // --- Если не нашли в TLB, ищем в таблице страниц ---
        if (frame < 0) {
            for (int x = 0; x < MIN(page_table_size_temp, COUNT_BACKING_STORE_PAGES); x++) {
                if (page_table[x][0] == page) {                     // Если страница найдена в таблице
                    frame = page_table[x][1];                       // Получаем номер кадра
                    value = memory[frame * PAGE_SIZE + offset];     // Читаем значение
                    break;
                }
            }

            // --- Если не нашли в таблице страниц — страница отсутствует в памяти ---
            if (frame < 0) {
                int free_page = -1;                                 // Ищем свободный кадр в памяти

                // --- Поиск свободного кадра ---
                for (int x = 0; x < COUNT_MEMORY_PAGES; x++) {
                    if (!claim_table[x][0]) {                       // Если кадр свободен (не занят)
                        free_page = x;                              // Используем его
                        claim_table[x][0] = 1;                      // Помечаем как занятый
                        break;
                    }
                }

                // --- Если свободных кадров нет — необходимо замещение страницы ---
                if (free_page < 0) {
                    int count_use_min = claim_table[0][1];          // Инициализируем минимум по использованию
                    free_page = 0;

                    // --- Поиск наименее используемого кадра ---
                    for (int x = 1; x < COUNT_MEMORY_PAGES; x++) {
                        if (claim_table[x][1] <= count_use_min) {   // Если кадр использовался реже
                            count_use_min = claim_table[x][1];      // Обновляем минимум
                            free_page = x;                          // Обновляем кандидат на замещение
                        }
                    }

                    // --- Удаление страницы из таблицы страниц ---
                    for (int x = 0; x < MIN(page_table_size_temp, COUNT_BACKING_STORE_PAGES); x++) {
                        if (page_table[x][1] == free_page) {        // Если страница указывает на заменяемый кадр
                            page_table[x][0] = -1;                  // Очищаем запись
                            memcpy(page_table[x],
                                page_table[(page_table_size - 1) % COUNT_BACKING_STORE_PAGES],
                                sizeof(int) * 2);                // Затираем запись последней актуальной
                            page_table_size--;                      // Уменьшаем размер таблицы
                        }
                    }

                    // --- Удаление записи из TLB ---
                    for (int x = 0; x < MIN(tlb_size_temp, TLB_SIZE); x++) {
                        if (tlb_table[x][1] == free_page) {         // Если запись TLB указывает на удаляемый кадр
                            tlb_table[x][0] = -1;                   // Очищаем запись
                            memcpy(tlb_table[x],
                                tlb_table[(tlb_size - 1) % TLB_SIZE],
                                sizeof(int) * 2);                // Перезаписываем последней
                            tlb_size--;                             // Уменьшаем размер TLB
                        }
                    }
                }

                // --- Загрузка страницы из backing store в память ---
                frame = free_page;                                  // Используем найденный или заменённый кадр
                fseek(file, page * PAGE_SIZE, SEEK_SET);            // Смещаемся на нужную страницу в backing store
                fread(backing_store, 1, PAGE_SIZE, file);           // Считываем 1 страницу (256 байт)
                memcpy(memory + frame * PAGE_SIZE, backing_store, PAGE_SIZE); // Копируем в память

                // --- Обновление таблицы страниц ---
                page_table[page_table_size % COUNT_BACKING_STORE_PAGES][0] = page;   // Добавляем страницу
                page_table[page_table_size % COUNT_BACKING_STORE_PAGES][1] = frame;  // Сохраняем кадр
                page_table_size++;                                                   // Увеличиваем размер таблицы

                // --- Обновление TLB ---
                tlb_table[tlb_size % TLB_SIZE][0] = page;             // Добавляем страницу
                tlb_table[tlb_size % TLB_SIZE][1] = frame;            // Сохраняем кадр
                tlb_size++;                                           // Увеличиваем размер TLB

                // --- Обновляем счётчик использования кадра ---
                claim_table[frame][1]++;                              // Увеличиваем счётчик обращений

                // --- Читаем значение из памяти ---
                value = memory[frame * PAGE_SIZE + offset];           // Получаем значение по физическому адресу

                // --- Увеличиваем счётчик ошибок страниц ---
                page_faults++;                                        // Регистрируем page fault
            }
        }

        int phys_addr = frame * PAGE_SIZE + offset;                  // Вычисление физического адреса

        // --- Сохраняем результат в файл ---
        fprintf(result, "Virtual address: %d Physical address: %d Value: %d\n",
            virtual_address, phys_addr, value);                  // Запись информации в out.txt

        total++;                                                     // Увеличиваем общее число обработанных адресов
    }

    // --- Вычисление частоты попаданий в TLB ---
    float tlb_hit_rate = tlb_hits / (float)total;       // Делим количество попаданий в TLB на общее число адресов
    // Приведение к float нужно, чтобы результат был дробным (например, 0.055)

// --- Вычисление частоты ошибок страниц ---
    float page_fault_rate = page_faults / (float)total; // Аналогично: делим количество ошибок страниц на общее число адресов

    // --- Вывод статистики ---
    printf("\u0427\u0430\u0441\u0442\u043e\u0442\u0430 \u043f\u043e\u043f\u0430\u0434\u0430\u043d\u0438\u0439 \u0432 TLB: %f\n"
        "\u0427\u0430\u0441\u0442\u043e\u0442\u0430 \u043e\u0448\u0438\u0431\u043e\u043a \u0441\u0442\u0440\u0430\u043d\u0438\u0446: %f\n",
        tlb_hit_rate, page_fault_rate);

    // Текст выше представлен в виде Unicode-последовательностей (\uXXXX) — это эквивалент русских символов UTF-8:
    // \u0427 = Ч, \u0430 = а, \u0441 = с и т.д.
    // Таким образом, итоговая строка печатается на русском языке:
    // Частота попаданий в TLB: ...
    // Частота ошибок страниц: ...

    // Такой способ используется для избежания проблем с кодировкой в исходнике (особенно в системах без поддержки UTF-8),
    // или если редактор/компилятор не поддерживает русские символы.
    // %f — формат вывода для чисел с плавающей точкой (например, 0.123456)

    // --- Закрытие файлов ---
    fclose(file);        // Закрытие файла с backing_store (бинарный файл, где хранятся страницы)
    fclose(addreses);    // Закрытие файла с входными виртуальными адресами
    fclose(result);      // Закрытие выходного файла с результатами (out.txt)


    return 0;
}
