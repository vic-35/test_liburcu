#include <stdio.h>
#include <pthread.h>

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#define RCU_MEMBARRIER
#include <urcu.h>

static pthread_mutex_t m1;
static pthread_spinlock_t s1;

#define REZ1 0xffff0000
#define REZ2 0x0000ffff

unsigned int *result = NULL;

void *test(void *arg)
{
    long long i = (long long) arg;
    long long count = 0;
    long long count1 = 0;

    printf("start :%d\n", (int) i);

    while (pthread_mutex_trylock(&m1) == EBUSY) {
        pthread_spin_lock(&s1);
        usleep(1);
        if (i == 0) {
            unsigned int rez = *result;
            *result = 0x0;
            free(result);
            result = malloc(sizeof(unsigned int));
            if (rez == REZ1) {
                *result = REZ2;
                count++;
            } else {
                *result = REZ1;
                count1++;
            }
        } else {
            unsigned int rez = *result;
            if (rez != REZ1 && rez != REZ2) {
                printf("Fatal error :%lld :%lld :%lld\n", i, count, count1);
                exit(-1);
            }
            if (rez == REZ1) {
                count++;
            } else {
                count1++;
            }
        }
        pthread_spin_unlock(&s1);
    }
    pthread_mutex_unlock(&m1);
    printf("end :%lld :%lld :%lld\n", i, count, count1);

    pthread_exit(NULL);
}

void *test_rcu(void *arg)
{
    long long i = (long long) arg;
    long long count = 0;
    long long count1 = 0;
    rcu_register_thread();

    printf("start :%d\n", (int) i);

    while (pthread_mutex_trylock(&m1) == EBUSY) {
        rcu_read_lock();
        usleep(1);
        if (i == 0) {
            unsigned int rez = *result;
            unsigned int *new_result = malloc(sizeof(unsigned int));
            if (rez == REZ1) {
                *new_result = REZ2;
                count++;
            } else {
                *new_result = REZ1;
                count1++;
            }
            rcu_read_unlock();

            unsigned int *old_result = rcu_xchg_pointer(&result, new_result);
            synchronize_rcu();

            if (old_result)
                *old_result = 0;
            free(old_result);

        } else {
            unsigned int rez = *result;
            if (rez != REZ1 && rez != REZ2) {
                printf("Fatal error :%lld :%lld :%lld\n", i, count, count1);
                exit(-1);
            }
            if (rez == REZ1) {
                count++;

            } else {
                count1++;
            }
            rcu_read_unlock();
        }
    }
    pthread_mutex_unlock(&m1);
    printf("end :%lld :%lld :%lld\n", i, count, count1);

    rcu_unregister_thread();
    pthread_exit(NULL);
}

int main()
{
    pthread_mutex_init(&m1, NULL);
    pthread_spin_init(&s1, 0);

    result = malloc(sizeof(unsigned int));
    *result = REZ1;

    int num_threads = 6;

    long long i;

    pthread_t tid[num_threads];

    // ---------- SPIN-lock
    pthread_mutex_lock(&m1);
    for (i = 0; i < num_threads; i++) {
        pthread_create(&tid[i], NULL, &test, (void *) i);
    }

    sleep(10);
    pthread_mutex_unlock(&m1);

    for (i = 0; i < num_threads; i++) {
        pthread_join(tid[i], NULL);
    }

    // --------------URCU-------------------
    pthread_mutex_lock(&m1);
    for (i = 0; i < num_threads; i++) {
        pthread_create(&tid[i], NULL, &test_rcu, (void *) i);
    }

    sleep(10);
    pthread_mutex_unlock(&m1);

    for (i = 0; i < num_threads; i++) {
        pthread_join(tid[i], NULL);
    }

    free(result);
    pthread_spin_destroy(&s1);
    pthread_mutex_destroy(&m1);
}
