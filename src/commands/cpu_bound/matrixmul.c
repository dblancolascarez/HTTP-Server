// matrixmul.c - multiplica dos matrices N x N pseudoaleatorias y devuelve hash FNV-1a

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../../utils/utils.h"

// Generate pseudo-random matrix of doubles in [0,1)
static double* gen_matrix(int n, unsigned int seed) {
	double *m = malloc(sizeof(double) * n * n);
	if (!m) return NULL;
	srand(seed);
	for (int i = 0; i < n * n; i++) m[i] = (double)rand() / (double)RAND_MAX;
	return m;
}

// Free matrix
static void free_matrix(double* m) { free(m); }

// FNV-1a 64-bit hash over raw bytes
static void fnv1a_hash_bytes(const void* data, size_t len, unsigned char out_hex[17]) {
	const uint8_t *p = (const uint8_t*)data;
	uint64_t hash = 14695981039346656037ULL;
	for (size_t i = 0; i < len; i++) {
		hash ^= p[i];
		hash *= 1099511628211ULL;
	}
	// convert to hex
	for (int i = 0; i < 8; i++) {
		sprintf((char*)out_hex + i*2, "%02x", (unsigned int)((hash >> (56 - i*8)) & 0xFF));
	}
	out_hex[16] = '\0';
}

char* handle_matrixmul(const char* size_str, const char* seed_str) {
	if (!size_str || !seed_str) return NULL;
	int n = atoi(size_str);
	unsigned int seed = (unsigned int)atoi(seed_str);
	if (n <= 0 || n > 400) return strdup("{\"error\":\"Invalid size (1..400)\"}");

	http_timer_t timer; timer_start(&timer);

	double *A = gen_matrix(n, seed);
	double *B = gen_matrix(n, seed + 1);
	double *C = calloc(n * n, sizeof(double));
	if (!A || !B || !C) { free_matrix(A); free_matrix(B); free_matrix(C); return strdup("{\"error\":\"Memory allocation failed\"}"); }

	// Multiply C = A * B (naive)
	for (int i = 0; i < n; i++) {
		for (int k = 0; k < n; k++) {
			double aik = A[i*n + k];
			for (int j = 0; j < n; j++) {
				C[i*n + j] += aik * B[k*n + j];
			}
		}
	}

	// Hash result matrix bytes
	unsigned char hash_hex[17];
	fnv1a_hash_bytes((const void*)C, sizeof(double) * n * n, hash_hex);

	timer_stop(&timer);
	long elapsed = timer_elapsed_ms(&timer);

	char *json = malloc(256);
	if (!json) { free_matrix(A); free_matrix(B); free_matrix(C); return NULL; }
	snprintf(json, 256, "{\"size\":%d,\"seed\":%u,\"result_hash\":\"%s\",\"elapsed_ms\":%ld}", n, seed, hash_hex, elapsed);

	free_matrix(A); free_matrix(B); free_matrix(C);
	return json;
}