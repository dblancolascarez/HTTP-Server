// pi.c - cálculo aproximado de PI usando fórmulas en coma flotante
// Nota: para alta precisión (muchos dígitos) se recomienda instalar libgmp y
// usar una implementación con aritmética de precisión arbitraria.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../utils/utils.h"

// Compute arctan(x) using Taylor series; accurate for |x|<=1, converges faster
// for smaller x. We use long double for slightly better precision.
static long double arctan_ld(long double x, int terms) {
	long double xpow = x;
	long double sum = x;
	for (int n = 1; n < terms; n++) {
		xpow *= x * x;
		long double term = xpow / (2 * n + 1);
		if (n % 2 == 1) sum -= term; else sum += term;
	}
	return sum;
}

char* handle_pi(const char* digits_str) {
	if (!digits_str) return NULL;
	int digits = atoi(digits_str);
	if (digits <= 0) return strdup("{\"error\":\"Invalid digits parameter\"}");

	// Warn/limit: without GMP we can only reliably return ~15 digits with long double
	if (digits > 15) {
		return strdup("{\"error\":\"Requested digits >15; install libgmp and recompile for higher precision\"}");
	}

	http_timer_t timer; timer_start(&timer);

	// Machin-like formula: pi/4 = 4*arctan(1/5) - arctan(1/239)
	int terms = 40; // enough for double/long double precision
	long double a1 = arctan_ld(1.0L/5.0L, terms);
	long double a2 = arctan_ld(1.0L/239.0L, terms);
	long double pi_ld = 4.0L * (4.0L * a1 - a2);

	timer_stop(&timer);
	long elapsed = timer_elapsed_ms(&timer);

	// Format to requested digits
	char buf[128];
	// Create format like "%.Nd" where N = digits
	char fmt[16];
	snprintf(fmt, sizeof(fmt), "%%.%dLf", digits);
	snprintf(buf, sizeof(buf), fmt, pi_ld);

	char *json = malloc(256);
	if (!json) return NULL;
	snprintf(json, 256, "{\"digits\":%d,\"elapsed_ms\":%ld,\"pi\":\"%s\"}", digits, elapsed, buf);
	return json;
}