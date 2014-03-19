#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int global = 0;
pthread_mutex_t m1,m2;


void* f1(void* arg){
	int i;
	for (i=0;i<100;i++){
	pthread_mutex_lock(&m1);
	pthread_mutex_lock(&m2);
	printf("f1 %d\n", global++);
	pthread_mutex_unlock(&m2);
	pthread_mutex_unlock(&m1);
	printf("f1\n");
	}
	return 0;
}

void* f2(void* arg){
	int i;
	for (i=0;i<100;i++){
	pthread_mutex_lock(&m2);
	pthread_mutex_lock(&m1);
	printf("f2 %d\n",global++);
	pthread_mutex_unlock(&m1);
	pthread_mutex_unlock(&m2);
	printf("f2\n");
	}
	return 0;
}



int main(){
	pthread_t t1,t2;
	pthread_mutex_init(&m1, NULL);
	pthread_mutex_init(&m2, NULL);
	printf("%d\n",global++);
	pthread_create(&t1, NULL, f1, NULL);
	pthread_create(&t2, NULL, f2, NULL);
	
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	printf("%d\n",global);


	return 0;
}
