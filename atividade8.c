#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


/*Para compilar use: gcc -pthread -O2 -Wall atividade8.c -o atividade8
E para rodar use: ./atividade8 8 100000 */

/*Esse código foi feito usando uma marquina virtual com linux*/


typedef int item_t;
item_t *buf;
int capacity;
int head = 0, tail = 0, count = 0;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;
pthread_cond_t can_produce = PTHREAD_COND_INITIALIZER;

unsigned long long produced = 0;
unsigned long long consumed = 0;
unsigned long long total_items = 0;

int BURST_SIZE = 10;
int BURST_IDLE_MS = 100;
int HIGH_WM = -1;
int LOW_WM = -1;

void *producer(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&mtx);
        if (produced >= total_items) { pthread_mutex_unlock(&mtx); break; }

        while (HIGH_WM >= 0 && count > HIGH_WM)
            pthread_cond_wait(&can_produce, &mtx);

        int b = BURST_SIZE;
        while (b > 0 && produced < total_items) {
            while (count == capacity) pthread_cond_wait(&not_full, &mtx);
            buf[tail] = 1;
            tail = (tail + 1) % capacity;
            count++;
            produced++;
            pthread_cond_signal(&not_empty);
            b--;
            if (HIGH_WM >= 0 && count > HIGH_WM) break;
        }
        pthread_mutex_unlock(&mtx);
        usleep(BURST_IDLE_MS * 1000);
    }
    return NULL;
}

void *consumer(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&mtx);
        while (count == 0) {
            if (produced >= total_items) { pthread_mutex_unlock(&mtx); return NULL; }
            pthread_cond_wait(&not_empty, &mtx);
        }
        (void)buf[head];
        head = (head + 1) % capacity;
        count--;
        consumed++;
        pthread_cond_signal(&not_full);
        if (LOW_WM >= 0 && count <= LOW_WM) pthread_cond_signal(&can_produce);
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s BUFFER TOTAL_ITEMS [BURST_SIZE] [BURST_IDLE_MS] [HIGH_WM] [LOW_WM]\n", argv[0]);
        return 1;
    }

    capacity = atoi(argv[1]);
    total_items = atoll(argv[2]);
    if (capacity <= 0 || total_items == 0) { fprintf(stderr,"param invalid\n"); return 1; }
    if (argc >= 4) BURST_SIZE = atoi(argv[3]);
    if (argc >= 5) BURST_IDLE_MS = atoi(argv[4]);
    if (argc >= 6) HIGH_WM = atoi(argv[5]);
    if (argc >= 7) LOW_WM  = atoi(argv[6]);

    if (HIGH_WM < 0) HIGH_WM = capacity - 2;
    if (LOW_WM  < 0) LOW_WM  = capacity / 2;

    buf = malloc(capacity * sizeof(item_t));
    if (!buf) { perror("malloc"); return 1; }

    pthread_t pt, ct;
    pthread_create(&ct, NULL, consumer, NULL);
    pthread_create(&pt, NULL, producer, NULL);

    pthread_join(pt, NULL);

    pthread_mutex_lock(&mtx);
    pthread_cond_signal(&not_empty);
    pthread_cond_signal(&can_produce);
    pthread_mutex_unlock(&mtx);

    pthread_join(ct, NULL);

    printf("done: produced=%llu consumed=%llu\n", produced, consumed);

    free(buf);
    return 0;
}
