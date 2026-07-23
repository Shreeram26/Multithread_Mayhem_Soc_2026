/*
 * word_count.c — Week 1, Problem 3: Parallel word count
 *
 * Usage:
 *   ./word_count <filename> <num_threads>
 *
 * Reads the whole file into memory, splits it into num_threads
 * roughly-equal chunks (adjusted so a chunk boundary never lands in
 * the middle of a word), and has each thread count the words in its
 * chunk.
 *
 * A "word" here is defined as a maximal run of non-whitespace
 * characters (matches what `wc -w` counts).
 *
 * On purpose, per the assignment: every thread adds its partial count
 * directly into a SHARED `total` with no locking (`total += local;`
 * mirrors 03_parallel_sum.c). This is deliberately racy so the race
 * shows up in real work, not just a toy counter.
 *
 * Compile:
 *   gcc -Wall -pthread word_count.c -o word_count
 *
 * Run:
 *   ./word_count moby_dick.txt 4
 *   wc -w moby_dick.txt        # ground truth to compare against
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

long total = 0;   /* SHARED, unprotected on purpose */

typedef struct {
    const char *start;
    long        len;
} chunk_t;

int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

void *count_chunk(void *arg) {
    chunk_t *c = (chunk_t *)arg;
    long local = 0;
    int in_word = 0;

    for (long i = 0; i < c->len; i++) {
        if (is_space(c->start[i])) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            local++;
        }
    }

    total += local;   /* racy on purpose */
    return NULL;
}

char *read_whole_file(const char *filename, long *out_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        perror("malloc");
        fclose(f);
        return NULL;
    }
    size_t nread = fread(buf, 1, size, f);
    buf[nread] = '\0';
    fclose(f);

    *out_size = (long)nread;
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <num_threads>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    int num_threads = atoi(argv[2]);
    if (num_threads <= 0) {
        fprintf(stderr, "num_threads must be a positive integer\n");
        return 1;
    }

    long size;
    char *data = read_whole_file(filename, &size);
    if (!data) return 1;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    chunk_t   *chunks   = malloc(num_threads * sizeof(chunk_t));

    long base_chunk = size / num_threads;
    long pos = 0;

    for (int i = 0; i < num_threads; i++) {
        long end;
        if (i == num_threads - 1) {
            end = size;   /* last thread takes whatever remains */
        } else {
            end = pos + base_chunk;
            if (end > size) end = size;
            /* Extend the chunk to the next whitespace so we never
             * split a word across two threads' chunks. */
            while (end < size && !is_space(data[end])) {
                end++;
            }
        }
        chunks[i].start = data + pos;
        chunks[i].len   = end - pos;
        pos = end;
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, count_chunk, &chunks[i]);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("File:        %s\n", filename);
    printf("Threads:     %d\n", num_threads);
    printf("Word count:  %ld\n", total);
    printf("(compare against: wc -w %s)\n", filename);

    free(data);
    free(threads);
    free(chunks);
    return 0;
}
