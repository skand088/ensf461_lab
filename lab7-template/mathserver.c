#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <ctype.h>

#define MAX_CTX 16

typedef struct Operation {
    char op[16];
    long long arg;
    struct Operation *next;
} Operation;

long long contexts[MAX_CTX];
Operation *queues[MAX_CTX];

pthread_t threads[MAX_CTX];
pthread_mutex_t queue_locks[MAX_CTX];
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

FILE *log_file;

int input_done = 0;
pthread_cond_t work_ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t work_lock = PTHREAD_MUTEX_INITIALIZER;


static void log_set(int ctx, long long val) {
    pthread_mutex_lock(&log_lock);
    fprintf(log_file, "ctx %02d: set to value %lld\n", ctx, val);
    pthread_mutex_unlock(&log_lock);
}

static void log_simple(int ctx, const char *op, long long operand, long long result) {
    pthread_mutex_lock(&log_lock);
    fprintf(log_file, "ctx %02d: %s %lld (result: %lld)\n", ctx, op, operand, result);
    pthread_mutex_unlock(&log_lock);
}

static void log_fib(int ctx, long long result) {
    pthread_mutex_lock(&log_lock);
    fprintf(log_file, "ctx %02d: fib (result: %lld)\n", ctx, result);
    pthread_mutex_unlock(&log_lock);
}

static void log_primes(int ctx, const char *list) {
    pthread_mutex_lock(&log_lock);
    fprintf(log_file, "ctx %02d: primes (result: %s)\n", ctx, list);
    pthread_mutex_unlock(&log_lock);
}

static void log_pia(int ctx, double res) {
    pthread_mutex_lock(&log_lock);
    fprintf(log_file, "ctx %02d: pia (result %.15f)\n", ctx, res);
    pthread_mutex_unlock(&log_lock);
}

long long fib_slow(long long n) {
    if (n <= 1) return n;
    return fib_slow(n - 1) + fib_slow(n - 2);
}

/* produce comma-separated primes <= n */
char* primes_str(long long n) {
    if (n < 2) {
        char *s = malloc(1);
        s[0] = '\0';
        return s;
    }

    // Start with reasonable buffer
    size_t cap = 1024;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    int first = 1;
    for (long long i = 2; i <= n; ++i) {
        int is_prime = 1;
        for (long long j = 2; j*j <= i; ++j) {
            if (i % j == 0) { is_prime = 0; break; }
        }
        if (is_prime) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%lld", i);
            size_t needed = strlen(out) + strlen(tmp) + (first ? 1 : 3); // comma + space + null
            while (needed > cap) {
                cap *= 2;
                out = realloc(out, cap);
                if (!out) return NULL;
            }

            if (!first) strcat(out, ", ");
            first = 0;
            strcat(out, tmp);
        }
    }
    return out;
}


double pi_leibniz(long long iters) {
    if (iters <= 0) return 0.0;
    double sum = 0.0;
    for (long long k = 0; k < iters; ++k) {
        double term = 1.0 / (2.0 * (double)k + 1.0);
        if (k % 2) sum -= term; else sum += term;
    }
    return 4.0 * sum;
}

void enqueue(int ctx, const char *op, long long arg) {
    if (ctx < 0 || ctx >= MAX_CTX) return;
    Operation *node = malloc(sizeof(Operation));
    if (!node) return;
    strncpy(node->op, op, sizeof(node->op)-1);
    node->op[sizeof(node->op)-1] = '\0';
    node->arg = arg;
    node->next = NULL;

    pthread_mutex_lock(&queue_locks[ctx]);
    if (!queues[ctx]) queues[ctx] = node;
    else {
        Operation *cur = queues[ctx];
        while (cur->next) cur = cur->next;
        cur->next = node;
    }
    pthread_mutex_unlock(&queue_locks[ctx]);
}

#define LOG_BATCH 10

typedef struct {
    char *lines[LOG_BATCH];
    int count;
} LogBatch;

void* worker(void *arg) {
    int ctx = *(int*)arg;
    LogBatch batch = { .count = 0 };

    while (1) {
        pthread_mutex_lock(&queue_locks[ctx]);
        while (queues[ctx] == NULL && !input_done) {
            pthread_mutex_unlock(&queue_locks[ctx]);
            pthread_mutex_lock(&work_lock);
            pthread_cond_wait(&work_ready, &work_lock);
            pthread_mutex_unlock(&work_lock);
            pthread_mutex_lock(&queue_locks[ctx]);
        }

        Operation *op = queues[ctx];
        if (!op) {
            pthread_mutex_unlock(&queue_locks[ctx]);
            break;
        }
        queues[ctx] = op->next;
        pthread_mutex_unlock(&queue_locks[ctx]);

        char *line = NULL;  // dynamically allocated log line
        long long val = op->arg;

        if (strcmp(op->op, "set") == 0) {
            contexts[ctx] = val;
            size_t len = 50;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: set to value %lld\n", ctx, contexts[ctx]);
        }
        else if (strcmp(op->op, "add") == 0) {
            contexts[ctx] += val;
            size_t len = 70;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: add %lld (result: %lld)\n", ctx, val, contexts[ctx]);
        }
        else if (strcmp(op->op, "sub") == 0) {
            contexts[ctx] -= val;
            size_t len = 70;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: sub %lld (result: %lld)\n", ctx, val, contexts[ctx]);
        }
        else if (strcmp(op->op, "mul") == 0) {
            contexts[ctx] *= val;
            size_t len = 70;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: mul %lld (result: %lld)\n", ctx, val, contexts[ctx]);
        }
        else if (strcmp(op->op, "div") == 0) {
            if (val != 0) contexts[ctx] /= val;
            size_t len = 70;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: div %lld (result: %lld)\n", ctx, val, contexts[ctx]);
        }
        else if (strcmp(op->op, "fib") == 0) {
            long long res = fib_slow(contexts[ctx]);
            size_t len = 70;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: fib (result: %lld)\n", ctx, res);
        }
        else if (strcmp(op->op, "pri") == 0) {
            char *list = primes_str(contexts[ctx]);
            size_t len = strlen(list) + 50;  // dynamic size for full prime list
            line = malloc(len);
            snprintf(line, len, "ctx %02d: primes (result: %s)\n", ctx, list);
            free(list);
        }
        else if (strcmp(op->op, "pia") == 0) {
            double res = pi_leibniz(contexts[ctx]);
            size_t len = 80;
            line = malloc(len);
            snprintf(line, len, "ctx %02d: pia (result %.15f)\n", ctx, res);
        }

        free(op);

        // Add line to batch
        if (line) {
            batch.lines[batch.count++] = line;
        }

        // Write batch if full
        if (batch.count == LOG_BATCH) {
            pthread_mutex_lock(&log_lock);
            for (int i = 0; i < batch.count; ++i) {
                fputs(batch.lines[i], log_file);
                free(batch.lines[i]);
            }
            pthread_mutex_unlock(&log_lock);
            batch.count = 0;
        }
    }

    // Flush remaining lines
    if (batch.count > 0) {
        pthread_mutex_lock(&log_lock);
        for (int i = 0; i < batch.count; ++i) {
            fputs(batch.lines[i], log_file);
            free(batch.lines[i]);
        }
        pthread_mutex_unlock(&log_lock);
    }

    return NULL;
}


/* trim leading/trailing whitespace */
static char* trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

int main(int argc, char* argv[]) {
    const char usage[] = "Usage: mathserver.out <input trace> <output trace>\n";
    if (argc != 3) {
        printf("%s", usage);
        return 1;
    }

    FILE *input_file = fopen(argv[1], "r");
    log_file = fopen(argv[2], "w");
    if (!input_file || !log_file) {
        fprintf(stderr, "Error opening files.\n");
        return 1;
    }

    /* init contexts and mutexes */
    for (int i = 0; i < MAX_CTX; ++i) {
        contexts[i] = 0;
        queues[i] = NULL;
        pthread_mutex_init(&queue_locks[i], NULL);
    }

    int used_contexts[MAX_CTX] = {0};
    int context_order[MAX_CTX];
    int order_count = 0;

    /* read input file and enqueue operations */
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), input_file)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        char *line = trim(buffer);
        if (line[0] == '\0' || line[0] == '#') continue;

        int ctx = -1;
        char op[16] = {0};
        long long val = 0;
        int parts = sscanf(line, "%15s %d %lld", op, &ctx, &val);
        if (parts >= 2) {
            for (char *p = op; *p; ++p) *p = (char)tolower((unsigned char)*p);
            if (ctx < 0 || ctx >= MAX_CTX) continue;

            enqueue(ctx, op, val);

            /* track first appearance for deterministic order */
            if (!used_contexts[ctx]) {
                used_contexts[ctx] = 1;
                context_order[order_count++] = ctx;
            }
        }
    }

    /* spawn workers in order of first appearance */
    int ids[MAX_CTX];
    for (int i = 0; i < order_count; ++i) {
        ids[i] = context_order[i];
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    /* signal workers that input is complete and wake them up */
    pthread_mutex_lock(&work_lock);
    input_done = 1;
    pthread_cond_broadcast(&work_ready);
    pthread_mutex_unlock(&work_lock);

    /* join all threads */
    for (int i = 0; i < order_count; ++i) {
        pthread_join(threads[i], NULL);
    }

    fclose(input_file);
    fclose(log_file);

    /* destroy mutexes and cond */
    for (int i = 0; i < MAX_CTX; ++i) pthread_mutex_destroy(&queue_locks[i]);
    pthread_mutex_destroy(&log_lock);
    pthread_mutex_destroy(&work_lock);
    pthread_cond_destroy(&work_ready);

    return 0;
}
