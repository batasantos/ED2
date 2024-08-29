#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#define ID_TAM 3           // Tamanho fixo do ID do aluno (3 caracteres)
#define SIGLA_TAM 3        // Tamanho fixo da sigla da disciplina (3 caracteres)
#define NOME_TAM 50        // Tamanho máximo dos nomes (50 caracteres)
#define TAM_FLT sizeof(float)  // Tamanho do tipo float
#define HEADER_SIZE (sizeof(int) + sizeof(long))  // Tamanho do cabeçalho (int para tamanho + long para offset)

// Estrutura para armazenar os dados de um registro
typedef struct {
    char id_aluno[ID_TAM + 1];  // ID do aluno, com espaço para o terminador nulo
    char sigla_disciplina[SIGLA_TAM + 1];  // Sigla da disciplina, com espaço para o terminador nulo
    char nome_aluno[NOME_TAM + 1];  // Nome do aluno, com espaço para o terminador nulo
    char nome_disciplina[NOME_TAM + 1];  // Nome da disciplina, com espaço para o terminador nulo
    float media;  // Média do aluno na disciplina
    float frequencia;  // Frequência do aluno na disciplina
} Registro;

// Estrutura para o cabeçalho do arquivo
typedef struct {
    long offset_lista_livre;  // Offset para o início da lista de espaços livres
} Cabecalho;

// Função para inserir um registro no arquivo
void inserir_registro(FILE *file, Registro *registro, Cabecalho *cabecalho) {
    long offset = cabecalho->offset_lista_livre;  // Obtém o offset do próximo espaço livre
    long pos = ftell(file);  // Posição atual no arquivo

    if (offset == -1) {
        // Se não há espaços livres, insere no final do arquivo
        fseek(file, 0, SEEK_END);
        pos = ftell(file);
    } else {
        // Se há um espaço livre, usa-o
        fseek(file, offset, SEEK_SET);
        int tamanho;
        long next_offset;
        fread(&tamanho, sizeof(int), 1, file);
        fread(&next_offset, sizeof(long), 1, file);

        // Atualiza o espaço livre atual com o novo tamanho e offset
        fseek(file, offset, SEEK_SET);
        tamanho = sizeof(int) + sizeof(long) + ID_TAM + 1 + SIGLA_TAM + 1 + NOME_TAM + 1 + NOME_TAM + 1 + TAM_FLT + TAM_FLT;
        fwrite(&tamanho, sizeof(int), 1, file);
        fwrite(&cabecalho->offset_lista_livre, sizeof(long), 1, file);

        // Atualiza o cabeçalho com o próximo offset livre
        cabecalho->offset_lista_livre = next_offset;
        fseek(file, 0, SEEK_SET);
        fwrite(cabecalho, sizeof(Cabecalho), 1, file);
    }

    // Escreve o novo registro no arquivo
    fwrite(registro->id_aluno, ID_TAM + 1, 1, file);
    fwrite(registro->sigla_disciplina, SIGLA_TAM + 1, 1, file);
    fwrite(registro->nome_aluno, NOME_TAM + 1, 1, file);
    fwrite(registro->nome_disciplina, NOME_TAM + 1, 1, file);
    fwrite(&registro->media, TAM_FLT, 1, file);
    fwrite(&registro->frequencia, TAM_FLT, 1, file);
}

// Função para remover um registro do arquivo
void remover_registro(FILE *file, const char *id_aluno, const char *sigla_disciplina, Cabecalho *cabecalho) {
    long pos = -1;
    long offset = cabecalho->offset_lista_livre;

    // Percorre a lista de espaços livres para encontrar o registro a ser removido
    while (offset != -1) {
        fseek(file, offset, SEEK_SET);
        int tamanho;
        long next_offset;
        fread(&tamanho, sizeof(int), 1, file);
        fread(&next_offset, sizeof(long), 1, file);

        fseek(file, sizeof(int) + sizeof(long), SEEK_CUR);
        char id[ID_TAM + 1];
        char sigla[SIGLA_TAM + 1];
        fread(id, ID_TAM + 1, 1, file);
        fread(sigla, SIGLA_TAM + 1, 1, file);

        // Verifica se o registro corresponde ao ID e sigla fornecidos
        if (strcmp(id, id_aluno) == 0 && strcmp(sigla, sigla_disciplina) == 0) {
            pos = offset;
            break;
        }

        // Move para o próximo espaço livre
        offset = next_offset;
    }

    // Se o registro foi encontrado, marca o espaço como livre
    if (pos != -1) {
        fseek(file, pos, SEEK_SET);
        int tamanho = sizeof(int) + sizeof(long) + ID_TAM + 1 + SIGLA_TAM + 1 + NOME_TAM + 1 + NOME_TAM + 1 + TAM_FLT + TAM_FLT;
        fwrite(&tamanho, sizeof(int), 1, file);
        fwrite(&cabecalho->offset_lista_livre, sizeof(long), 1, file);
        cabecalho->offset_lista_livre = pos;
        fseek(file, 0, SEEK_SET);
        fwrite(cabecalho, sizeof(Cabecalho), 1, file);
    }
}

// Função para compactar o arquivo removendo espaços vazios
void compactar_arquivo(const char *nome_arquivo) {
    FILE *file = fopen(nome_arquivo, "r+b");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        return;
    }

    FILE *temp_file = fopen("temp.bin", "w+b");
    if (!temp_file) {
        perror("Erro ao abrir o arquivo temporário");
        fclose(file);
        return;
    }

    Cabecalho cabecalho;
    fread(&cabecalho, sizeof(Cabecalho), 1, file);
    long offset = cabecalho.offset_lista_livre;
    long pos = HEADER_SIZE;

    // Reescreve o arquivo sem os espaços vazios
    while (offset != -1) {
        fseek(file, offset, SEEK_SET);
        int tamanho;
        long next_offset;
        fread(&tamanho, sizeof(int), 1, file);
        fread(&next_offset, sizeof(long), 1, file);

        fseek(file, sizeof(int) + sizeof(long), SEEK_CUR);
        char id[ID_TAM + 1];
        char sigla[SIGLA_TAM + 1];
        fread(id, ID_TAM + 1, 1, file);
        fread(sigla, SIGLA_TAM + 1, 1, file);

        fseek(file, sizeof(int) + sizeof(long) + ID_TAM + 1 + SIGLA_TAM + 1, SEEK_CUR);
        char buffer[256];
        fread(buffer, 1, 256, file);
        fwrite(&tamanho, sizeof(int), 1, temp_file);
        fwrite(&pos, sizeof(long), 1, temp_file);
        fwrite(buffer, 1, tamanho - sizeof(int) - sizeof(long), temp_file);

        pos = ftell(temp_file);
        offset = next_offset;
    }

    fclose(file);
    fclose(temp_file);

    remove(nome_arquivo);
    rename("temp.bin", nome_arquivo);
}

// Função para carregar registros de inserção a partir de um arquivo
void carregar_insercoes(const char *arquivo, FILE *data_file, Cabecalho *cabecalho) {
    FILE *insercao_file = fopen(arquivo, "r");
    if (!insercao_file) {
        perror("Erro ao abrir arquivo de inserção");
        return;
    }

    Registro registro;
    // Lê cada linha do arquivo de inserção e adiciona ao arquivo de dados
    while (fscanf(insercao_file, "%[^#]#%[^#]#%[^#]#%[^#]#%f#%f\n",
                  registro.id_aluno, registro.sigla_disciplina,
                  registro.nome_aluno, registro.nome_disciplina,
                  &registro.media, &registro.frequencia) == 6) {
        inserir_registro(data_file, &registro, cabecalho);
    }

    fclose(insercao_file);
}

// Função para carregar registros de remoção a partir de um arquivo
void carregar_remocoes(const char *arquivo, FILE *data_file, Cabecalho *cabecalho) {
    FILE *remocao_file = fopen(arquivo, "r");
    if (!remocao_file) {
        perror("Erro ao abrir arquivo de remoção");
        return;
    }

    char id_aluno[ID_TAM + 1];
    char sigla_disciplina[SIGLA_TAM + 1];
    // Lê cada linha do arquivo de remoção e remove o registro correspondente
    while (fscanf(remocao_file, "%[^#]#%[^\n]\n", id_aluno, sigla_disciplina) == 2) {
        remover_registro(data_file, id_aluno, sigla_disciplina, cabecalho);
    }

    fclose(remocao_file);
}

int main() {
    FILE *file;
    Cabecalho cabecalho;

    // Configura a localidade para suportar caracteres especiais
    setlocale(LC_ALL, "");

    // Abre o arquivo principal de dados (historico.dad)
    file = fopen("historico.dad", "r+b");
    if (!file) {
        // Se o arquivo não existir, cria um novo
        file = fopen("historico.dad", "w+b");
        if (!file) {
            perror("Erro ao abrir o arquivo");
            return EXIT_FAILURE;
        }
        cabecalho.offset_lista_livre = -1;  // Inicializa a lista de espaços livres como vazia
        fwrite(&cabecalho, sizeof(Cabecalho), 1, file);
    } else {
        fread(&cabecalho, sizeof(Cabecalho), 1, file);  // Lê o cabeçalho do arquivo
    }

    int opcao;
    do {
        printf("Escolha uma opcao:\n");
        printf("1. Insercao\n");
        printf("2. Remocao\n");
        printf("3. Compactacao\n");
        printf("4. Carregar Arquivos\n");
        printf("0. Sair\n");
        scanf("%d", &opcao);
        getchar();  // Limpa o newline após scanf

        switch (opcao) {
            case 1: {
                // Carrega e insere registros a partir do arquivo de inserção
                carregar_insercoes("insere.bin", file, &cabecalho);
                break;
            }
            case 2: {
                // Carrega e remove registros a partir do arquivo de remoção
                carregar_remocoes("remove.bin", file, &cabecalho);
                break;
            }
            case 3: {
                // Compacta o arquivo para remover espaços vazios
                compactar_arquivo("historico.dad");
                break;
            }
            case 4: {
                // Se necessário, carregar arquivos de inserção e remoção pode ser feito aqui
                break;
            }
            case 0:
                // Sai do programa
                break;
            default:
                printf("Opcao invalida.\n");
        }

        // Atualiza o cabeçalho do arquivo após cada operação
        fseek(file, 0, SEEK_SET);
        fwrite(&cabecalho, sizeof(Cabecalho), 1, file);

    } while (opcao != 0);

    fclose(file);  // Fecha o arquivo de dados
    return 0;
}
