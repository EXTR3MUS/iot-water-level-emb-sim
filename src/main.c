#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void* read_sensor(void *arg);

int main(){
    pthread_t thread_1, thread_2;

    pthread_create(&thread_1, NULL, &read_sensor, NULL);
    pthread_join(thread_1, NULL);

    return 0;
}

//Thread 1 - Ler informações do sensor ultrassônico (nível de água no reservatório);
//Thread 1 - Executar esta leitura a cada 5 minutos;
void* read_sensor(void *arg){
    srand(time(NULL)); // Seed the random number generator

    while(1){
         int random_number = (rand() % 100) + 1; // 1 to 100

        sleep(5);
        printf("Current level: %d \n", random_number);
    }
}

//Thread 2 - Gravar os dados lidos em arquivo local (buffer);

//Thread 3 - Enviar dados armazenados para o servidor IoT (central de monitoramento) via rede;
//Thread 3 - Realizar o envio a cada 5 minutos;
//Thread 3 - Sempre enviar todos os dados do buffer (arquivo) no momento do envio;

//Thread 4 - Esvaziar o buffer (arquivo local) somente em caso de sucesso no envio dos dados para o servidor IoT;

//Thread 5 - Determinar o Status "normal" (Nível ≥ 35%);
//Thread 5 - Determinar o Status "alerta" (Nível entre 20% e 35%);
//Thread 5 - Determinar o Status "crítico/falta de água" (Nível < 20%);
