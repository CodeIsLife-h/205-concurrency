// hotdog_manager.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>

// ---------------------- Work Simulation ----------------------

void do_work(int n) {
    for (int i = 0; i < n; i++) {
        long m = 300000000L;
        while (m > 0) m--;
    }
}

// ---------------------- Shared State -------------------------

typedef struct {
    int hotdog_id;   // 1..N
    int maker_id;    // 0-based index of maker (for logging add 1)
} Hotdog;

int N;  // total hotdogs to make
int S;  // buffer size
int M;  // makers
int P;  // packers

Hotdog *buffer = NULL;
int buf_size = 0;
int buf_count = 0;
int buf_front = 0;
int buf_back = 0;

int next_hotdog_id = 1; // 1..N
int total_produced = 0;
int total_packed = 0;
int production_done = 0; // set when all makers have finished

int *maker_counts = NULL;   // how many each maker produced
int *packer_counts = NULL;  // how many each packer packed

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *log_file = NULL;

// ---------------------- Logging Helper -----------------------

void log_write(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    pthread_mutex_lock(&log_mutex);
    vfprintf(log_file, fmt, args);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    va_end(args);
}

// ---------------------- Maker Thread -------------------------

void *maker_thread(void *arg) {
    int maker_id = *(int *)arg;  // 0-based

    while (1) {
        // 1) Make a hotdog: 4 time units
        do_work(4);

        pthread_mutex_lock(&buffer_mutex);

        // Check if we already produced N hotdogs
        if (total_produced >= N) {
            pthread_mutex_unlock(&buffer_mutex);
            break;
        }

        // Assign a new hotdog ID
        int hotdog_id = next_hotdog_id++;
        total_produced++;

        // Wait if buffer is full
        while (buf_count == buf_size) {
            pthread_cond_wait(&not_full, &buffer_mutex);
        }

        // 2) Send hotdog to pool: 1 time unit
        // (we simulate sending while holding the lock so that the slot
        // "belongs" to this hotdog during the sending time)
        do_work(1);

        // Insert into circular buffer
        buffer[buf_back].hotdog_id = hotdog_id;
        buffer[buf_back].maker_id = maker_id;
        buf_back = (buf_back + 1) % buf_size;
        buf_count++;

        maker_counts[maker_id]++;

        // Notify packers that a hotdog is available
        pthread_cond_signal(&not_empty);

        pthread_mutex_unlock(&buffer_mutex);

        // 3) After sending, log the event
        log_write("m%d puts %d\n", maker_id + 1, hotdog_id);
        // Once this log is written, the maker will loop back and start
        // making the next hotdog.
    }

    return NULL;
}

// ---------------------- Packer Thread ------------------------

void *packer_thread(void *arg) {
    int packer_id = *(int *)arg;  // 0-based

    while (1) {
        pthread_mutex_lock(&buffer_mutex);

        // Wait while buffer empty, production not done, and not all packed
        while (buf_count == 0 && !production_done && total_packed < N) {
            pthread_cond_wait(&not_empty, &buffer_mutex);
        }

        // If there is nothing to pack and production is done (or we've
        // already packed N), we can stop.
        if (buf_count == 0 && (production_done || total_packed >= N)) {
            pthread_mutex_unlock(&buffer_mutex);
            break;
        }

        // There is at least one hotdog in the pool.

        // 1) Take 1 time unit to take a hotdog from the pool
        do_work(1);

        int hotdog_id = buffer[buf_front].hotdog_id;
        int maker_id  = buffer[buf_front].maker_id;
        buf_front = (buf_front + 1) % buf_size;
        buf_count--;

        total_packed++;
        packer_counts[packer_id]++;

        // Signal makers that there is space
        pthread_cond_signal(&not_full);

        pthread_mutex_unlock(&buffer_mutex);

        // 2) Pack the hotdog: 2 time units
        do_work(2);

        // 3) After packing, log the event
        log_write("p%d gets %d from m%d\n",
                  packer_id + 1, hotdog_id, maker_id + 1);
        // Once this log is written, the packer will loop back and look for
        // the next hotdog.
    }

    return NULL;
}

// ---------------------- Main / Manager -----------------------

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <N> <S> <M> <P>\n", argv[0]);
        fprintf(stderr, "  N = total hot dogs to produce\n");
        fprintf(stderr, "  S = buffer size\n");
        fprintf(stderr, "  M = number of making machines\n");
        fprintf(stderr, "  P = number of packing machines\n");
        return 1;
    }

    N = atoi(argv[1]);
    S = atoi(argv[2]);
    M = atoi(argv[3]);
    P = atoi(argv[4]);

    if (N <= 0 || S <= 0 || M <= 0 || P <= 0) {
        fprintf(stderr, "Error: all arguments must be positive integers\n");
        return 1;
    }

    if (S >= N) {
        fprintf(stderr, "Error: S (buffer slots) must be less than N (hotdogs)\n");
        return 1;
    }

    if (M > 30 || P > 30) {
        fprintf(stderr, "Error: M and P must be <= 30\n");
        return 1;
    }

    buf_size = S;

    buffer = (Hotdog *)malloc(sizeof(Hotdog) * buf_size);
    maker_counts = (int *)calloc(M, sizeof(int));
    packer_counts = (int *)calloc(P, sizeof(int));

    if (!buffer || !maker_counts || !packer_counts) {
        fprintf(stderr, "Error: memory allocation failed\n");
        free(buffer);
        free(maker_counts);
        free(packer_counts);
        return 1;
    }

    log_file = fopen("log.txt", "w");
    if (!log_file) {
        fprintf(stderr, "Error: cannot open log.txt for writing\n");
        free(buffer);
        free(maker_counts);
        free(packer_counts);
        return 1;
    }

    // Header in log file
    log_write("order:%d\n", N);
    log_write("capacity:%d\n", S);
    log_write("making machines:%d\n", M);
    log_write("packing machines:%d\n", P);
    log_write("-----\n");

    // Create threads
    pthread_t *makers = (pthread_t *)malloc(sizeof(pthread_t) * M);
    pthread_t *packers = (pthread_t *)malloc(sizeof(pthread_t) * P);
    int *maker_ids = (int *)malloc(sizeof(int) * M);
    int *packer_ids = (int *)malloc(sizeof(int) * P);

    if (!makers || !packers || !maker_ids || !packer_ids) {
        fprintf(stderr, "Error: memory allocation failed\n");
        fclose(log_file);
        free(buffer);
        free(maker_counts);
        free(packer_counts);
        free(makers);
        free(packers
