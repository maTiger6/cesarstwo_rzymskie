#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define N 20
#define Q 10
#define R 10

typedef struct dzialka_t
{
    pthread_mutex_t mtx;
    int worki;
    int posypane;
} dzialka_t;

typedef struct signalHandler_args_t
{
    pthread_t tid;
    sigset_t *pMask;
    pthread_t* threads;
} signalHandler_args_t;

void* tragarz_work(void* arg)
{
    dzialka_t* dzialki = (dzialka_t*) arg;
    int next;
    struct timespec ts = {0, 0};
    while(1)
    {
        next = rand() % N;
        ts.tv_nsec = (5 + next)*1e6;
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&dzialki[next].mtx);
        dzialki[next].worki++;
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
    }

    pthread_t tragarze[Q];

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

    for(int i = 0; i < Q; i++)
    {
        if(0 != pthread_create(&tragarze[i], NULL, tragarz_work, dzialki))
            ERR("pthread_create()");
    }

    for(int i = 0; i < 10; i++)
    {
        sleep(1);
        for(int i = 0; i < N; i++)
        {
            pthread_mutex_lock(&dzialki[i].mtx);
            printf("%d ", dzialki[i].worki);
            pthread_mutex_unlock(&dzialki[i].mtx);
        }
        printf("\n");
    }
    printf("Sending signal\n");
    kill(0, SIGTERM);
    printf("Signal sent\n");
    for(int i = 0; i < N; i++)
    {
        if(0 != pthread_join(tragarze[i], NULL))
            ERR("pthread_join()");
    }
    printf("Joined\n");
    return 0;
}