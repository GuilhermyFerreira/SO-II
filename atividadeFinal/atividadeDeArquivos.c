#include <stdio.h>
#include <stdlib.h>

int main (){
    FILE *arq;
    arq = fopen("disco.bin", "rb+");
    if (arq=NULL){
        perror("Erro ao abrir o arquivo");
        return 1;
    }
fclose(arq);
}