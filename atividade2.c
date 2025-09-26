#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
/*Para compilar use: gcc -pthread -O2 -Wall atividade2.c -o atividade2
E para rodar use: ./atividade2 8 100000 */
/*Esse código foi feito usando uma marquina virtual com linux*/

typedef int item_t;
item_t *buf;
int capacity;
int head = 0;
int tail = 0;
int count = 0;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;
uint64_t produced = 0;
uint64_t consumed = 0;
uint64_t total_items = 0;

void *producer(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&mtx);
        if (produced >= total_items) {
            pthread_mutex_unlock(&mtx);
            break;
        }
        while (count == capacity) {
            pthread_cond_wait(&not_full, &mtx);
        }
        buf[tail] = 1;
        tail = (tail + 1) % capacity;
        count++;
        produced++;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

void *consumer(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&mtx);
        if (consumed >= total_items && count == 0) {
            pthread_mutex_unlock(&mtx);
            break;
        }
        while (count == 0) {
            if (produced >= total_items) {
                pthread_mutex_unlock(&mtx);
                goto end;
            }
            pthread_cond_wait(&not_empty, &mtx);
        }
        (void)buf[head];
        head = (head + 1) % capacity;
        count--;
        consumed++;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mtx);
    }
end:
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <BUFFER_SIZE> <TOTAL_ITEMS>\n", argv[0]);
        return 1;
    }
    capacity = atoi(argv[1]);
    total_items = (uint64_t)atoll(argv[2]);
    if (capacity <= 0 || total_items == 0) {
        fprintf(stderr, "Parametros invalidos\n");
        return 1;
    }

    buf = malloc((size_t)capacity * sizeof(item_t));
    if (!buf) { perror("malloc"); return 1; }

    pthread_t p, c;
    pthread_create(&p, NULL, producer, NULL);
    pthread_create(&c, NULL, consumer, NULL);

    pthread_join(p, NULL);

    pthread_mutex_lock(&mtx);
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&mtx);

    pthread_join(c, NULL);

    printf("done: produced=%" PRIu64 " consumed=%" PRIu64 "\n", produced, consumed);

    free(buf);
    return 0;
}
