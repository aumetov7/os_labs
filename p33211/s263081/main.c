#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

//A=100;B=0xF56A669C;C=mmap;D=138;E=104;F=block;G=136;H=random;I=56;J=avg;K=cv

#define MEMORY_SIZE (100 * 1024 * 1024)
#define ADDRESS (void*)0xF56A669C
#define WRITE_THREADS_SIZE 138
#define IO_BLOCK 136
#define ERROR_MMAP_MEMORY -11
#define FILE_SIZE 104 * 1024 * 1024
#define READ_THREADS_AMOUNT 56

void work_with_memory();

void create_for_memory_threads();

void* put_data_in_memory(void* args);

pthread_t* write_to_file();

pthread_t* read_from_file();

char* memory_pointer = NULL;
const char* filename = "file.bin";
pthread_mutex_t* mutex = NULL;
pthread_cond_t* cond = NULL;
int fd = 0;
bool isWriting = false;

void init_file() {
    fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0600);
    if (fd == -1) {
        perror("Ошибка при создании файла для записи и чтения\n");
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    cond = malloc(sizeof(pthread_cond_t));
    mutex = malloc(sizeof(pthread_mutex_t));
    pthread_cond_init(cond, NULL);
    pthread_mutex_init(mutex, NULL);

    while (1) {
        printf("Сделайте замер памяти до аллокации. После этого нажмите на Enter.\n");
        getchar();
        work_with_memory();

        init_file();
        pthread_t* write_thread = write_to_file();
        pthread_t* read_threads = read_from_file();

        pthread_join(*write_thread, NULL);
        for (int i = 0; i < READ_THREADS_AMOUNT; i++) {
            pthread_join(read_threads[i], NULL);
        }

        free(write_thread);
        free(read_threads);
        munmap(memory_pointer, MEMORY_SIZE);
    }

    return 0;
}

void work_with_memory() {
    memory_pointer = mmap(ADDRESS, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (memory_pointer == MAP_FAILED) {
        printf("При аллокации произошла ошибка, пожалуйста попробуйте снова чуть позже.\n");
        exit(ERROR_MMAP_MEMORY);
    }

    printf("Сделайте замер паяти после аллокации. После этого нажмите на Enter.\n");
    printf("Адресс начала аллоцируемой памяти = %p\n", memory_pointer);
    getchar();

    create_for_memory_threads();
    printf("Сделайте замер памяти после заполнения аллоцируемой памяти. После этого нажмите на Enter.\n");
    getchar();

    printf("Работа с памятью закончена.\nСделайте замер памяти после деаллокации. После этого нажмите на Enter.\n");
    getchar();
}

void create_for_memory_threads() {
    pthread_t threads[WRITE_THREADS_SIZE];
    int threads_id[WRITE_THREADS_SIZE];
    for (int i = 0; i < WRITE_THREADS_SIZE; i++) {
        threads_id[i] = i;
        pthread_create(&threads[i], NULL, put_data_in_memory, (void*) &threads_id[i]);
    }
    for (int i = 0; i < WRITE_THREADS_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }
}

void* put_data_in_memory(void* args) {
    int id = *(int*) args;
    int block = MEMORY_SIZE / WRITE_THREADS_SIZE;
    char* start_of_block = memory_pointer + block * id;
    char* end_of_block = (id == WRITE_THREADS_SIZE) ? (memory_pointer + MEMORY_SIZE) : (memory_pointer + block * (id + 1));

    int urandom;
    int tmp = IO_BLOCK;
    char buffer[IO_BLOCK];

    if ((urandom = open("/dev/urandom", O_RDONLY)) < 0) {
        printf("Невозможно открыть /dev/urandom\n");
        exit(EXIT_FAILURE);
    }

    for (char* current_address = start_of_block; current_address < end_of_block; current_address++) {
        if (tmp == IO_BLOCK) {
            read(urandom, buffer, IO_BLOCK);
            tmp = 0;
        }
        *current_address = buffer[tmp++];
    }

    close(urandom);

    return NULL;
}

void* write_to_file_thread() {
    printf("Поток номер %lu начал запись в файл\n", (uint64_t) pthread_self());
    char* io_block = malloc(sizeof(char) * IO_BLOCK);

    for (size_t offset = 0; offset < MEMORY_SIZE; offset += IO_BLOCK) {
//        printf("%lu/%d\n", offset, MEMORY_SIZE);
        if (offset + IO_BLOCK >= MEMORY_SIZE) {
            memcpy(io_block, memory_pointer + offset, MEMORY_SIZE - offset);
        } else {
            memcpy(io_block, memory_pointer + offset, IO_BLOCK);
        }


        pthread_mutex_lock(mutex);
        isWriting = true;

        ssize_t bytesWritten = write(fd, io_block, IO_BLOCK);
        if (bytesWritten == -1) {
            perror("Ошибка при записи в файл");
            return NULL;
        }

        isWriting = false;
        pthread_cond_signal(cond);
        pthread_mutex_unlock(mutex);
    }

    free(io_block);
    printf("Поток номер %lu закончил запись в файл\n", (uint64_t) pthread_self());
    return NULL;
}

pthread_t* write_to_file() {
    pthread_t* write_thread = malloc(sizeof(pthread_t));
    pthread_create(write_thread, NULL, write_to_file_thread, NULL);
    return write_thread;
}

void* read_from_file_thread() {
    float sum = 0;
    float n = 0;
    printf("Поток номер %lu начал читать из файла\n", (uint64_t) pthread_self());

    pthread_mutex_lock(mutex);
    lseek(fd, 0, SEEK_SET);
    pthread_mutex_unlock(mutex);

    char* io_block = malloc(sizeof(char) * IO_BLOCK);
    for (int i = 0; i < FILE_SIZE / IO_BLOCK; i++) {
        pthread_mutex_lock(mutex);
        while (isWriting) {
            pthread_cond_wait(cond, mutex);
        }

        ssize_t readBytes = read(fd, io_block, IO_BLOCK);
        if (readBytes == -1) {
            perror("Ошибка при записи в файл");
            return NULL;
        }

        for (int j = 0; j < IO_BLOCK; j++) {
            sum += (unsigned char) io_block[j];
            n += 1;
        }

        pthread_mutex_unlock(mutex);
    }
    float avg = sum / n;
    printf("Среднее значение в файле: %f\n", avg);

    free(io_block);
    printf("Поток номер %lu закончил читать из файла\n", (uint64_t) pthread_self());
    return NULL;
}

pthread_t* read_from_file() {
    pthread_t* read_threads = malloc(READ_THREADS_AMOUNT * sizeof(pthread_t));

    for (int i = 0; i < READ_THREADS_AMOUNT; i++) {
        pthread_create(&read_threads[i], NULL, read_from_file_thread, NULL);
    }

    return read_threads;
}
