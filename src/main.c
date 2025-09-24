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

void* read_sensor(void *arg){
    srand(time(NULL)); // Seed the random number generator

    while(1){
         int random_number = (rand() % 100) + 1; // 1 to 100

        sleep(5);
        printf("Current level: %d \n", random_number);
    }
}