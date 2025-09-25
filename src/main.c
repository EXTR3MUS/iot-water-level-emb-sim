#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void* read_sensor(void *arg);
void* send_data(void *arg);

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread1, thread2;

int random_number = 0;

int main(){
    pthread_t thread_1, thread_2;

    pthread_create(&thread_1, NULL, &read_sensor, NULL);
    pthread_create(&thread_2, NULL, &send_data, NULL);

    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);

    return 0;
}

// items 1 and 2
void* read_sensor(void *arg){
    srand(time(NULL)); // Seed the random number generator

    while(1){
        pthread_mutex_lock(&mutex1);
        random_number  = (rand() % 100) + 1; // 1 to 100
        pthread_mutex_unlock(&mutex1);  
        sleep(5);
    }
}

void* send_data(void *arg){
    while(1){
        pthread_mutex_lock(&mutex1);
        printf("Current level: %d \n", random_number);
        pthread_mutex_unlock(&mutex1);
        sleep(5);
    }
}