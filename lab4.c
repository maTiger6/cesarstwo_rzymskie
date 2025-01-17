#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define N 20
#define Q 10
#define R 10

typedef struct dzialka_t
{
    pthread_mutex_t mtx;
    int worki;
    int posypane;
    pthread_cond_t cv;
} dzialka_t;

typedef struct signalHandler_args_t
{
    pthread_t tid;
    sigset_t *pMask;
    pthread_t* threads;
} signalHandler_args_t;

typedef struct tragarz_args_t
{
    dzialka_t* dzialki;
    sem_t sem;
} tragarz_args_t;


void* tragarz_work(void* arg)
{
    tragarz_args_t* args = (tragarz_args_t*) arg;
    int next;
    struct timespec ts = {0, 0}, ts_port = {0, 1.5e7};
    while(1)
    {
        sem_wait(&args->sem);
        nanosleep(&ts_port, NULL);
        sem_post(&args->sem);

        next = rand() % N;
        ts.tv_nsec = (5 + next)*1e6;
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&args->dzialki[next].mtx);
        args->dzialki[next].worki++;
        pthread_mutex_unlock(&args->dzialki[next].mtx);
        pthread_cond_signal(&args->dzialki[next].cv);
    }
    return NULL;
}

void* robol_work(void* arg)
{
    dzialka_t* dzialki = (dzialka_t*) arg;
    int next, ret_val;
    struct timespec ts = {0, 5e8};
    while(1)
    {
        ret_val = 0;
        next = rand() % N;
        pthread_mutex_lock(&dzialki[next].mtx);
        while(!( dzialki[next].worki > 0 && dzialki[next].posypane < 50 ) && ret_val == 0)
            ret_val = pthread_cond_timedwait(&dzialki[next].cv, &dzialki[next].mtx, &ts);
        if(ret_val == 0)
        {
            dzialki[next].worki--;
            dzialki[next].posypane++;
        }
        pthread_mutex_unlock(&dzialki[next].mtx);
    }
    return NULL;
}

void* signal_handler(void* voidArgs)
{
    signalHandler_args_t* args = (signalHandler_args_t*)voidArgs;
    int signo;
    for (;;)
    {
        if (sigwait(args->pMask, &signo))
            ERR("sigwait failed.");
        switch (signo)
        {
            case SIGTERM:
                for(int j = 0; j < Q; j++)
                    pthread_cancel(args->threads[j]);
                exit(0);
        }
    }
    return NULL;
}

int main(int argc, char** argv)
{
    srand(time(NULL));
    dzialka_t dzialki[N];
    for(int i = 0; i < N; i++)
    {
        dzialki[i].worki = 0;
        dzialki[i].posypane = 0;
        if(0 != pthread_mutex_init(&dzialki[i].mtx, NULL))
            ERR("pthread_mutex_init()");
        if(0 != pthread_cond_init(&dzialki[i].cv, NULL))
            ERR("pthread_cond_init()");
    }

    pthread_t tragarze[Q];
    pthread_t robole[R];

#pragma region setting_signalHandler
    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    signalHandler_args_t sh_args; 
    sh_args.pMask = &newMask;
    sh_args.threads = tragarze;

    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&sh_args.tid, &tattr, signal_handler, &sh_args))
        ERR("Couldn't create signal handling thread!");
#pragma endregion setting_signalHandler

#pragma region thread_creation
    tragarz_args_t tragarz_args;
    tragarz_args.dzialki = dzialki;
    if(0 != sem_init(&tragarz_args.sem, 0, 3))
        ERR("sem_init()");
    for(int i = 0; i < Q; i++)
    {
        if(0 != pthread_create(&tragarze[i], NULL, tragarz_work, &tragarz_args))
            ERR("pthread_create()");
    }
    for(int i = 0; i < R; i++)
    {
        if(0 != pthread_create(&robole[i], NULL, robol_work, dzialki))
            ERR("pthread_create()");
    }
#pragma endregion thread_creation

#pragma region result_printing
    struct timespec ts = {0, 7e8};
    printf("Printing every 0.%ds\n\n", (int)(ts.tv_nsec/1e8) );
    for(int i = 0; i < 10; i++)
    {
        nanosleep(&ts, NULL);
        printf("Czekające: ");
        for(int i = 0; i < N; i++)
        {
            pthread_mutex_lock(&dzialki[i].mtx);
            printf("%d ", dzialki[i].worki);
            pthread_mutex_unlock(&dzialki[i].mtx);
        }
        printf("\nPosypane: ");
        for(int i = 0; i < N; i++)
        {
            pthread_mutex_lock(&dzialki[i].mtx);
            printf("%d ", dzialki[i].posypane);
            pthread_mutex_unlock(&dzialki[i].mtx);
        }
        printf("\n\n");
    }
#pragma endregion result_printing
    
    printf("Sending signal\n");
    kill(0, SIGTERM);
    printf("Signal sent\n");
    for(int i = 0; i < Q; i++)
    {
        if(0 != pthread_join(tragarze[i], NULL))
            ERR("pthread_join()");
    }
    for(int i = 0; i < R; i++)
    {
        if(0 != pthread_join(robole[i], NULL))
            ERR("pthread_join()");
    }

    printf("Joined\n");
    return 0;
}