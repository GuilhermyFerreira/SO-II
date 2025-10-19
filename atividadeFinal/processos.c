#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    int *shared_var = (int *)shmat(shmid, NULL, 0);
    *shared_var = 0;

    pid_t pid = fork();
    if (pid == 0) {
        // Processo filho
        printf("Filho: Incrementando a variável compartilhada.\n");
        (*shared_var)++;
        printf("Filho: Novo valor = %d\n", *shared_var);
        shmdt(shared_var);
        exit(0);
    } else {
        // Processo pai
        wait(NULL);
        printf("Pai: Valor final da variável compartilhada = %d\n", *shared_var);
        shmdt(shared_var);
        shmctl(shmid, IPC_RMID, NULL);
    }
    return 0;
}