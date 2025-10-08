#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>

void* read_sensor(void *arg);
void* send_data(void *arg);
void* save_data(void *arg);

sem_t semaphore1;
pthread_t thread1, thread2;

int random_number = 0;

int main(){
    pthread_t thread_1, thread_2, thread_3;

    if (sem_init(&semaphore1, 0, 1) != 0) {
        perror("Semaphore initialization failed");
        return 1;
    }

    pthread_create(&thread_1, NULL, &read_sensor, NULL);
    pthread_create(&thread_2, NULL, &send_data, NULL);
    pthread_create(&thread_3, NULL, &save_data, NULL);

    pthread_join(thread_1, NULL);
    pthread_join(thread_2, NULL);
    pthread_join(thread_3, NULL);

    sem_destroy(&semaphore1);

    return 0;
}

// thread 1
void* read_sensor(void *arg){
    srand(time(NULL)); // Seed the random number generator

    while(1){
        sem_wait(&semaphore1);
        random_number  = (rand() % 100) + 1; // 1 to 100
        printf("Reading data: Current level: %d \n", random_number);
        sem_post(&semaphore1);
        sleep(5);
    }
}

// thread 2
void* save_data(void *arg){
    while(1){
        sem_wait(&semaphore1);
        printf("Saving data: Current level: %d \n", random_number);
        sem_post(&semaphore1);
        sleep(5);
    }
}

// thread 3
void* send_data(void *arg){
    while(1){
        sem_wait(&semaphore1);
        printf("Sending data: Current level: %d \n", random_number);
        sem_post(&semaphore1);
        sleep(5);
    }
}
