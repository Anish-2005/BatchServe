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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define DINER_COUNT 25
#define BATCH_SIZE 5

#define ARRIVAL_DELAY_MAX_MS 1200
#define SERVICE_TIME_MIN_SEC 1
#define SERVICE_TIME_MAX_SEC 3
#define STARVATION_WARN_MS 5000

#define COLOR_RESET "\x1b[0m"
#define COLOR_INFO "\x1b[36m"
#define COLOR_ENTER "\x1b[32m"
#define COLOR_SERVE "\x1b[33m"
#define COLOR_LEAVE "\x1b[35m"
#define COLOR_BATCH "\x1b[34m"
#define COLOR_WARN "\x1b[31m"

static sem_t front_door;
static sem_t back_door;
static sem_t fair_queue[DINER_COUNT];
static pthread_mutex_t state_mutex;
static pthread_mutex_t log_mutex;

static int inside_count = 0;
static int current_batch_number = 1;
static int next_ticket_to_assign = 0;

typedef struct {
    int diner_id;
} DinerArgs;

static long current_time_ms(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

static void log_colored(const char *color, const char *format, ...) {
    va_list args;

    pthread_mutex_lock(&log_mutex);
    printf("%s", color);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("%s\n", COLOR_RESET);
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
}

static void animate_service(int batch_id, int service_seconds) {
    static const char spinner[] = {'|', '/', '-', '\\'};
    const int frames = service_seconds * 8;

    for (int i = 0; i < frames; i++) {
        pthread_mutex_lock(&log_mutex);
        printf("%s[BATCH %02d] Serving diners... %c%s\r", COLOR_SERVE, batch_id, spinner[i % 4], COLOR_RESET);
        fflush(stdout);
        pthread_mutex_unlock(&log_mutex);
        usleep(125000);
    }

    pthread_mutex_lock(&log_mutex);
    printf("%s[BATCH %02d] Serving finished.%s            \n", COLOR_SERVE, batch_id, COLOR_RESET);
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
}

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

    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
        perror("pthread_mutex_init(log_mutex) failed");
        pthread_mutex_destroy(&state_mutex);
        sem_destroy(&front_door);
        sem_destroy(&back_door);
        return -1;
    }

    for (int i = 0; i < DINER_COUNT; i++) {
        if (sem_init(&fair_queue[i], 0, 0) != 0) {
            perror("sem_init(fair_queue) failed");
            for (int j = 0; j < i; j++) {
                sem_destroy(&fair_queue[j]);
            }
            pthread_mutex_destroy(&log_mutex);
            pthread_mutex_destroy(&state_mutex);
            sem_destroy(&front_door);
            sem_destroy(&back_door);
            return -1;
        }
    }

    /* Start FIFO entry with ticket 0. */
    sem_post(&fair_queue[0]);

    return 0;
}

static void cleanup_system(void) {
    for (int i = 0; i < DINER_COUNT; i++) {
        sem_destroy(&fair_queue[i]);
    }
    pthread_mutex_destroy(&log_mutex);
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

    int ticket = 0;
    long queue_start_ms = current_time_ms();

    pthread_mutex_lock(&state_mutex);
    ticket = next_ticket_to_assign++;
    pthread_mutex_unlock(&state_mutex);

    log_colored(COLOR_INFO, "Diner %d queued with ticket %d", diner_id, ticket);

    sem_wait(&fair_queue[ticket]);

    sem_wait(&front_door);

    if (ticket + 1 < DINER_COUNT) {
        sem_post(&fair_queue[ticket + 1]);
    }

    int is_last_to_enter = 0;
    int batch_number_for_log = 0;
    int snapshot_inside = 0;
    long waited_ms = current_time_ms() - queue_start_ms;

    pthread_mutex_lock(&state_mutex);
    inside_count++;
    batch_number_for_log = current_batch_number;
    snapshot_inside = inside_count;

    if (inside_count == BATCH_SIZE) {
        is_last_to_enter = 1;
    }
    pthread_mutex_unlock(&state_mutex);

    log_colored(
        COLOR_ENTER,
        "[BATCH %02d] Diner %d entered (inside=%d, ticket=%d, wait=%ldms)",
        batch_number_for_log,
        diner_id,
        snapshot_inside,
        ticket,
        waited_ms
    );

    if (waited_ms > STARVATION_WARN_MS) {
        log_colored(
            COLOR_WARN,
            "[BATCH %02d] Starvation guard: Diner %d waited %ldms, FIFO queue ensured service",
            batch_number_for_log,
            diner_id,
            waited_ms
        );
    }

    if (is_last_to_enter) {
        log_colored(COLOR_BATCH, "[BATCH %02d] Batch full, serving started", batch_number_for_log);
    }

    if (is_last_to_enter) {
        int service_time = random_in_range(&seed, SERVICE_TIME_MIN_SEC, SERVICE_TIME_MAX_SEC);
        animate_service(batch_number_for_log, service_time);

        for (int i = 0; i < BATCH_SIZE; i++) {
            sem_post(&back_door);
        }
    }

    sem_wait(&back_door);

    int remaining_inside = 0;
    int is_last_to_leave = 0;

    pthread_mutex_lock(&state_mutex);
    inside_count--;
    remaining_inside = inside_count;

    if (inside_count == 0) {
        is_last_to_leave = 1;
        current_batch_number++;
    }
    pthread_mutex_unlock(&state_mutex);

    log_colored(COLOR_LEAVE, "[BATCH %02d] Diner %d leaving (remaining=%d)", batch_number_for_log, diner_id, remaining_inside);

    if (is_last_to_leave) {
        log_colored(COLOR_BATCH, "[BATCH %02d] Batch completed", batch_number_for_log);

        for (int i = 0; i < BATCH_SIZE; i++) {
            sem_post(&front_door);
        }
    }

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

    log_colored(COLOR_INFO, "Simulation started: %d diners, batch size %d", DINER_COUNT, BATCH_SIZE);

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

    log_colored(COLOR_INFO, "Simulation finished.");

    cleanup_system();
    return EXIT_SUCCESS;
}
