#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <sys/wait.h>

#define SHM_NAME "/mem_compartilhada"
#define SEM_PROD "/sem_produtor"
#define SEM_CONS "/sem_consumidor"

typedef struct {
    int dado;
    int pronto; // flag de controle (0 = vazio, 1 = cheio)
} MemoriaCompartilhada;

int main() {
    int fd;
    MemoriaCompartilhada *mem;
    sem_t *sem_prod, *sem_cons;

    // 1. Cria memória compartilhada
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("Erro ao criar memória compartilhada");
        exit(1);
    }

    ftruncate(fd, sizeof(MemoriaCompartilhada)); // define tamanho
    mem = mmap(NULL, sizeof(MemoriaCompartilhada),
               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (mem == MAP_FAILED) {
        perror("Erro ao mapear memória");
        exit(1);
    }

    // Inicializa a estrutura
    mem->dado = 0;
    mem->pronto = 0;

    // 2. Cria semáforos nomeados
    sem_prod = sem_open(SEM_PROD, O_CREAT, 0666, 1); // começa liberado
    sem_cons = sem_open(SEM_CONS, O_CREAT, 0666, 0); // começa bloqueado

    if (sem_prod == SEM_FAILED || sem_cons == SEM_FAILED) {
        perror("Erro ao criar semáforos");
        exit(1);
    }

    printf("Iniciando sistema produtor-consumidor com memória compartilhada...\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("Erro no fork");
        exit(1);
    }

    if (pid == 0) {
        // ----- Processo FILHO: CONSUMIDOR -----
        for (int i = 0; i < 5; i++) {
            sem_wait(sem_cons); // espera o produtor produzir

            printf("[Consumidor] Leu valor: %d\n", mem->dado);
            mem->pronto = 0; // marca como lido

            sem_post(sem_prod); // libera o produtor
            sleep(1);
        }
        exit(0);
    } else {
        // ----- Processo PAI: PRODUTOR -----
        for (int i = 0; i < 5; i++) {
            sem_wait(sem_prod); // espera permissão para produzir

            mem->dado = rand() % 100; // gera dado aleatório
            mem->pronto = 1;
            printf("[Produtor] Produziu valor: %d\n", mem->dado);

            sem_post(sem_cons); // libera o consumidor
            sleep(1);
        }

        wait(NULL); // espera o consumidor terminar

        // ----- Limpeza -----
        sem_close(sem_prod);
        sem_close(sem_cons);
        sem_unlink(SEM_PROD);
        sem_unlink(SEM_CONS);

        munmap(mem, sizeof(MemoriaCompartilhada));
        shm_unlink(SHM_NAME);

        printf("\nExecução finalizada com sucesso.\n");
    }

    return 0;
}