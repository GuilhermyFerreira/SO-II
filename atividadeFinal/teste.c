#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define BLOCK_SIZE 4096
#define BYTES_PER_LINE 16

// Função para imprimir a representação ASCII de um buffer
void print_ascii(const unsigned char *buffer, size_t size) {
    printf(" |");
    for (size_t i = 0; i < size; i++) {
        if (isprint(buffer[i])) {
            printf("%c", buffer[i]);
        } else {
            printf(".");
        }
    }
    printf("|");
}

int main(int argc, char *argv[]) {
    // 1. Verificação de argumentos
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <caminho_do_disco> <indice_do_bloco>\n", argv[0]);
        return 1;
    }

    const char *disk_path = argv[1];
    long block_index = strtol(argv[2], NULL, 10);

    // 2. Validação do índice do bloco
    if (block_index < 0) {
        fprintf(stderr, "Erro: O índice do bloco não pode ser negativo.\n");
        return 1;
    }

    // 3. Abertura do arquivo
    FILE *file = fopen(disk_path, "rb");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível abrir o arquivo '%s'.\n", disk_path);
        return 1;
    }

    // 4. Posicionamento no arquivo
    if (fseek(file, block_index * BLOCK_SIZE, SEEK_SET) != 0) {
        fprintf(stderr, "Erro: Não foi possível posicionar no índice do bloco %ld.\n", block_index);
        fclose(file);
        return 1;
    }

    unsigned char buffer[BLOCK_SIZE];
    size_t bytes_read = fread(buffer, 1, BLOCK_SIZE, file);

    // 5. Preencher com zeros se necessário
    if (bytes_read < BLOCK_SIZE) {
        memset(buffer + bytes_read, 0, BLOCK_SIZE - bytes_read);
    }

    fclose(file);

    // 6. Impressão do conteúdo formatado
    for (size_t i = 0; i < BLOCK_SIZE; i += BYTES_PER_LINE) {
        // Offset
        printf("%08lX: ", i + (block_index * BLOCK_SIZE));

        // Bytes em hexadecimal
        for (size_t j = 0; j < BYTES_PER_LINE; j++) {
            printf("%02X ", buffer[i + j]);
        }

        // Representação ASCII
        print_ascii(&buffer[i], BYTES_PER_LINE);
        printf("\n");
    }

    return 0;
}