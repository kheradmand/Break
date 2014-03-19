#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>


static int inMutex = 0;

static pthread_t mutexOwner;
static pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void sqlite3UnixEnterMutex(){

//      printf("%d waiting for m1\n", gettid());
  pthread_mutex_lock(&mutex1);
//    printf("%d holding m1\n", gettid());
  if( inMutex==0 ){
//        printf("%d waiting for m2\n", gettid());
	  printf("here\n");
    pthread_mutex_lock(&mutex2);
    mutexOwner = pthread_self();
  }
  pthread_mutex_unlock(&mutex1);


//  printf("%d holding m2\n", gettid());
  //sleep(2);
  inMutex++;
}

void sqlite3UnixLeaveMutex(){
  assert( inMutex>0 );

  //assert( pthread_equal(mutexOwner, pthread_self()) );
  pthread_mutex_lock(&mutex1);
  inMutex--;
  if( inMutex==0 ){
    pthread_mutex_unlock(&mutex2);
  }
  pthread_mutex_unlock(&mutex1);

}

void* work0(void* arg)
{
  sqlite3UnixEnterMutex();
  sqlite3UnixEnterMutex();

  printf("work0 \n");

  sqlite3UnixLeaveMutex();
  sqlite3UnixLeaveMutex();
  return 0;
}

void* work1(void* arg)
{
          //sleep(1);
  sqlite3UnixEnterMutex();

  printf("work1 \n");

  sqlite3UnixLeaveMutex();
  return 0;
}


int main(int argc, char* argv[])
{
  pthread_t t0, t1;
  int rc;

  rc = pthread_create (&t0, 0, work0, 0);
  rc = pthread_create (&t1, 0, work1, 0);


  pthread_join(t0, 0);
  pthread_join(t1, 0);

  return 0;
}
