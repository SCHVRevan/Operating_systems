#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// структура для данных потока
typedef struct{
	pthread_cond_t cond;
	pthread_mutex_t lock;
	int ready;
	int counter;
} pthrData;

pthrData global_data = {
	.cond = PTHREAD_COND_INITIALIZER,
	.lock = PTHREAD_MUTEX_INITIALIZER,
	.ready = 0,
	.counter = 0,
};

void* provide(void* arg) {
    while (1) {
        pthread_mutex_lock(&global_data.lock);
        if (global_data.ready == 1){
            pthread_mutex_unlock(&global_data.lock);
            printf("Not provided: событие не обработанно\n");
            continue;
        }
        global_data.ready = 1;
        printf("Provided\n");
        pthread_cond_signal(&global_data.cond);
        pthread_mutex_unlock(&global_data.lock);
        if (++global_data.counter >= 5) {
        	break;
        }
        sleep(1);
    }
    return NULL;
}

void* consume(void* arg) {
    while (1) {
        pthread_mutex_lock(&global_data.lock);
        while (global_data.ready == 0) {
            pthread_cond_wait(&global_data.cond, &global_data.lock);
            printf("Awoke\n");
        }
        global_data.ready = 0;
        printf("Consumed\n\n");
        pthread_mutex_unlock(&global_data.lock);
        if (global_data.counter >= 5) {
        	break;
        }
    }
    return NULL;
}

int main() {
    pthread_t producer_thread, consumer_thread;
    pthread_create(&producer_thread, NULL, provide, NULL);
    pthread_create(&consumer_thread, NULL, consume, NULL);
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);
    return 0;
}
