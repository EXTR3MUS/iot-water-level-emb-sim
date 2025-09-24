#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/*
	Esse programa apresenta o uso de threads e para exemplificar 
	temos threads que imprimem na tela "Thread 1" "Thread 2" "Thread 3"
	de forma alternada e aleatória (conforme escalonamento).
*/

/* 
	Funções que serão executadas pelas threads
	Essas funções recebem ponteiros para void, que são ponteiros para 
	"qualquer coisa", mas nesse exemplo não iremos usá-los.
*/
void* print_message_function1(void *arg);
void* print_message_function2(void *arg);
void* print_message_function3(void *arg);

// Espaço para declaração de varivéis globais (vistas por todas as threads).

int main()
{
	pthread_t thread1, thread2, thread3;
     
    /* Cria threads independentes, que irão executar funções */

	pthread_create( &thread1, NULL, &print_message_function1, NULL);
	pthread_create( &thread2, NULL, &print_message_function2, NULL);
	pthread_create( &thread3, NULL, &print_message_function3, NULL);
	 
	/*
	-> Cada thread é representada por um identificador (thread identifier 
	ou tid) que é do tipo thread_t. 
	-> É necessário passar o endereço de uma variável desse tipo, 
	como primeiro parâmetro de pthread_create() para receber o respectivo
	tid. 
	-> O segundo parâmetro indica as propriedades que a thread deverá ter. 
	O valor NULL indica que a thread terá as propriedades padrão, que 
	são adequadas na maior parte dos casos. 
	-> O terceiro parâmetro indica a função de início do thread. 	
	-> O quarto parâmetro (NULL) indica a função não recebe argumentos.
	*/


    /* Espera que as threads completem suas ações antes de continuar o 
	 main.
	*/

	pthread_join( thread1, NULL);
	pthread_join( thread2, NULL); 
	pthread_join( thread3, NULL); // NULL -> valor de retorno ignorado

    exit(0); // Quando as threads executam o programa encerra
}

void* print_message_function1(void *arg)
{
	// Laço infinito. A thread mostra a mensagem e "dorme" por 1 segundo.
	while(1) 
	{
		printf("Thread 1\n");
		sleep(1);
	}
}

void* print_message_function2(void *arg)
{
	while(1) // laço infinito
	{
		printf("Thread 2\n");
		sleep(1);
	}
}

void* print_message_function3(void *arg)
{	
	while(1) // laço infinito
	{
		printf("Thread 3\n");
		sleep(1);
	}
}