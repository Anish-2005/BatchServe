/*
 * Restaurant Batch Service Simulation (POSIX Threads + Semaphores)
 *
 * This program models a restaurant that serves diners in strict batches:
 * 1) Front door opens for exactly N diners.
 * 2) Once the batch is full, service starts.
 * 3) After service, back door opens and all N diners leave.
 * 4) Only when all N diners have exited does the next batch begin.
 *
 * Synchronization strategy:
 * - front_door semaphore: controls entry capacity per batch (N permits).
 * - back_door semaphore: blocks exiting until service is complete (N permits posted).
 * - mutex: protects shared counters and ensures race-free logging/state updates.
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define DINER_COUNT 25
#define BATCH_SIZE 5

#define ARRIVAL_DELAY_MAX_MS 1200
#define SERVICE_TIME_MIN_SEC 1
#define SERVICE_TIME_MAX_SEC 3

static sem_t front_door;
static sem_t back_door;
static pthread_mutex_t state_mutex;

static int inside_count = 0;
static int current_batch_number = 1;

typedef struct {
    int diner_id;
} DinerArgs;

static int random_in_range(unsigned int *seed, int min_val, int max_val) {
    if (max_val <= min_val) {
        return min_val;
    }
    return (rand_r(seed) % (max_val - min_val + 1)) + min_val;
}

static void sleep_ms(int milliseconds) {
    usleep((useconds_t)milliseconds * 1000);
}

static int initialize_system(void) {
    if (sem_init(&front_door, 0, BATCH_SIZE) != 0) {
        perror("sem_init(front_door) failed");
        return -1;
    }

    if (sem_init(&back_door, 0, 0) != 0) {
        perror("sem_init(back_door) failed");
        sem_destroy(&front_door);
        return -1;
    }

    if (pthread_mutex_init(&state_mutex, NULL) != 0) {
        perror("pthread_mutex_init failed");
        sem_destroy(&front_door);
        sem_destroy(&back_door);
        return -1;
    }

    return 0;
}

static void cleanup_system(void) {
    pthread_mutex_destroy(&state_mutex);
    sem_destroy(&front_door);
    sem_destroy(&back_door);
}

static void *diner_thread(void *arg) {
    DinerArgs *diner = (DinerArgs *)arg;
    int diner_id = diner->diner_id;

    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(diner_id * 2654435761u);

    int arrival_delay = random_in_range(&seed, 100, ARRIVAL_DELAY_MAX_MS);
    sleep_ms(arrival_delay);

    sem_wait(&front_door);

    int is_last_to_enter = 0;
    int batch_number_for_log = 0;

    pthread_mutex_lock(&state_mutex);
    inside_count++;
    batch_number_for_log = current_batch_number;

    printf("Diner %d entered (Batch %d, inside=%d)\n", diner_id, batch_number_for_log, inside_count);
    fflush(stdout);

    if (inside_count == BATCH_SIZE) {
        is_last_to_enter = 1;
        printf("Batch %d full, serving started\n", batch_number_for_log);
        fflush(stdout);
    }
    pthread_mutex_unlock(&state_mutex);

    if (is_last_to_enter) {
        int service_time = random_in_range(&seed, SERVICE_TIME_MIN_SEC, SERVICE_TIME_MAX_SEC);
        sleep(service_time);

        for (int i = 0; i < BATCH_SIZE; i++) {
            sem_post(&back_door);
        }
    }

    sem_wait(&back_door);

    pthread_mutex_lock(&state_mutex);
    inside_count--;
    printf("Diner %d leaving (Batch %d, remaining=%d)\n", diner_id, batch_number_for_log, inside_count);
    fflush(stdout);

    if (inside_count == 0) {
        printf("Batch %d completed\n", batch_number_for_log);
        fflush(stdout);
        current_batch_number++;

        for (int i = 0; i < BATCH_SIZE; i++) {
            sem_post(&front_door);
        }
    }
    pthread_mutex_unlock(&state_mutex);

    return NULL;
}

int main(void) {
    pthread_t diners[DINER_COUNT];
    DinerArgs diner_args[DINER_COUNT];

    if (DINER_COUNT % BATCH_SIZE != 0) {
        fprintf(stderr, "DINER_COUNT must be a multiple of BATCH_SIZE for exact full batches.\n");
        return EXIT_FAILURE;
    }

    if (initialize_system() != 0) {
        return EXIT_FAILURE;
    }

    printf("Simulation started: %d diners, batch size %d\n", DINER_COUNT, BATCH_SIZE);
    fflush(stdout);

    for (int i = 0; i < DINER_COUNT; i++) {
        diner_args[i].diner_id = i + 1;

        if (pthread_create(&diners[i], NULL, diner_thread, &diner_args[i]) != 0) {
            perror("pthread_create failed");

            for (int j = 0; j < i; j++) {
                pthread_join(diners[j], NULL);
            }
            cleanup_system();
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < DINER_COUNT; i++) {
        pthread_join(diners[i], NULL);
    }

    printf("Simulation finished.\n");
    fflush(stdout);

    cleanup_system();
    return EXIT_SUCCESS;
}
