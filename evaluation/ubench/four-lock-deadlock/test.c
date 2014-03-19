#include <pthread.h>
#include <stdio.h>

int global = 0;
pthread_mutex_t m1,m2,m3,m4;


void* f1(void* arg){
	pthread_mutex_lock(&m1);
	pthread_mutex_lock(&m2);
	global++;
	pthread_mutex_unlock(&m2);
	pthread_mutex_unlock(&m1);
	printf("f1\n");
	return 0;
}

void* f2(void* arg){
	pthread_mutex_lock(&m2);
	pthread_mutex_lock(&m3);
	global++;
	pthread_mutex_unlock(&m3);
	pthread_mutex_unlock(&m2);
	printf("f2\n");
	return 0;
}

void* f3(void* arg){
	pthread_mutex_lock(&m3);
	pthread_mutex_lock(&m4);
	global++;
	pthread_mutex_unlock(&m4);
	pthread_mutex_unlock(&m3);
	printf("f3\n");
	return 0;
}

void* f4(void* arg){
	pthread_mutex_lock(&m4);
	pthread_mutex_lock(&m1);
	global++;
	pthread_mutex_unlock(&m1);
	pthread_mutex_unlock(&m4);
	printf("f4\n");
	return 0;
}

int main(){
	pthread_t t1,t2,t3,t4;
	pthread_mutex_init(&m1, NULL);
	pthread_mutex_init(&m2, NULL);
	pthread_mutex_init(&m3, NULL);
	pthread_mutex_init(&m4, NULL);
	
	pthread_create(&t4, NULL, f1, NULL);
	pthread_create(&t3, NULL, f2, NULL);
	pthread_create(&t1, NULL, f3, NULL);
	pthread_create(&t2, NULL, f4, NULL);
	
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);
	pthread_join(t4, NULL);


	return 0;
}
