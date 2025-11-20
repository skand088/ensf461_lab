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
        char *s = malloc(2);
        s[0] = '\0';
        return s;
    }

    /* allocate a reasonably large buffer */
    size_t cap = 65536;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';

    int first = 1;
    for (long long i = 2; i <= n; ++i) {
        int is_prime = 1;
        for (long long j = 2; j * j <= i; ++j) {
            if (i % j == 0) { is_prime = 0; break; }
        }
        if (is_prime) {
            if (!first) strncat(out, ", ", cap - strlen(out) - 1);
            first = 0;
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%lld", i);
            strncat(out, tmp, cap - strlen(out) - 1);
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

void* worker(void *arg) {
    int ctx = *(int*)arg;

    while (1) {
        pthread_mutex_lock(&queue_locks[ctx]);
        Operation *op = queues[ctx];
        if (!op) {
            pthread_mutex_unlock(&queue_locks[ctx]);
            break; /* no more operations */
        }
        queues[ctx] = op->next;
        pthread_mutex_unlock(&queue_locks[ctx]);

        long long val = op->arg;

        if (strcmp(op->op, "set") == 0) {
            contexts[ctx] = val;
            log_set(ctx, contexts[ctx]);
        }
        else if (strcmp(op->op, "add") == 0) {
            contexts[ctx] += val;
            log_simple(ctx, "add", val, contexts[ctx]);
        }
        else if (strcmp(op->op, "sub") == 0) {
            contexts[ctx] -= val;
            log_simple(ctx, "sub", val, contexts[ctx]);
        }
        else if (strcmp(op->op, "mul") == 0) {
            contexts[ctx] *= val;
            log_simple(ctx, "mul", val, contexts[ctx]);
        }
        else if (strcmp(op->op, "div") == 0) {
            if (val == 0) {
                /* if divide by zero, perform no change but still log result format */
                log_simple(ctx, "div", val, contexts[ctx]);
            } else {
                contexts[ctx] /= val;
                log_simple(ctx, "div", val, contexts[ctx]);
            }
        }
        else if (strcmp(op->op, "fib") == 0) {
            long long n = contexts[ctx];
            long long res = fib_slow(n);
            log_fib(ctx, res);
        }
        else if (strcmp(op->op, "pri") == 0) {
            long long n = contexts[ctx];
            char *list = primes_str(n);
            if (!list) list = strdup("");
            log_primes(ctx, list);
            free(list);
        }
        else if (strcmp(op->op, "pia") == 0) {
            long long iters = contexts[ctx];
            double res = pi_leibniz(iters);
            log_pia(ctx, res);
        }
        /* unknown ops are silently ignored */
        free(op);
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

    /* init */
    for (int i = 0; i < MAX_CTX; ++i) {
        contexts[i] = 0;
        queues[i] = NULL;
        pthread_mutex_init(&queue_locks[i], NULL);
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), input_file)) {
        /* trim newline and whitespace */
        buffer[strcspn(buffer, "\n")] = '\0';
        char *line = trim(buffer);
        if (line[0] == '\0') continue;           /* skip blank */
        if (line[0] == '#') continue;            /* skip comment */

        int ctx = -1;
        char op[16] = {0};
        long long val = 0;

        /* FIX: parse op first, then ctx, then optional value */
        int parts = sscanf(line, "%15s %d %lld", op, &ctx, &val);
        /* parts:
           1 -> only op (not expected but skip)
           2 -> op ctx
           3 -> op ctx val
        */
        if (parts >= 2) {
            /* normalize op to lowercase */
            for (char *p = op; *p; ++p) *p = (char)tolower((unsigned char)*p);

            if (ctx < 0 || ctx >= MAX_CTX) {
                /* invalid context index: ignore line */
                continue;
            }

            /* if only op+ctx (parts==2), keep val as 0 - that's acceptable for ops that don't need arg */
            enqueue(ctx, op, val);
        }
    }

    /* spawn workers */
    int ids[MAX_CTX];
    for (int i = 0; i < MAX_CTX; ++i) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < MAX_CTX; ++i) {
        pthread_join(threads[i], NULL);
    }

    fclose(input_file);
    fclose(log_file);

    /* destroy mutexes */
    for (int i = 0; i < MAX_CTX; ++i) pthread_mutex_destroy(&queue_locks[i]);
    pthread_mutex_destroy(&log_lock);

    return 0;
}
