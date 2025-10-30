// mandelbrot.c - genera un mapa de iteraciones del conjunto de Mandelbrot

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <complex.h>
#include "../../utils/utils.h"

char* handle_mandelbrot(const char* width_str, const char* height_str, const char* max_iter_str) {
	if (!width_str || !height_str || !max_iter_str) return NULL;
	int width = atoi(width_str);
	int height = atoi(height_str);
	int max_iter = atoi(max_iter_str);
	if (width <= 0 || height <= 0 || max_iter <= 0) return strdup("{\"error\":\"Invalid parameters\"}");

	// Limit total pixels to avoid huge responses
	long total = (long)width * (long)height;
	if (total > 200000) return strdup("{\"error\":\"Requested image too large; limit width*height <= 200000\"}");

	http_timer_t timer; timer_start(&timer);

	int *iters = malloc(sizeof(int) * total);
	if (!iters) return strdup("{\"error\":\"Memory allocation failed\"}");

	double x_min = -2.0, x_max = 1.0;
	double y_min = -1.5, y_max = 1.5;
	double dx = (x_max - x_min) / (width - 1);
	double dy = (y_max - y_min) / (height - 1);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			double cx = x_min + x * dx;
			double cy = y_min + y * dy;
			double zx = 0.0, zy = 0.0;
			int iter = 0;
			while (zx*zx + zy*zy <= 4.0 && iter < max_iter) {
				double xt = zx*zx - zy*zy + cx;
				zy = 2.0*zx*zy + cy;
				zx = xt;
				iter++;
			}
			iters[y * width + x] = iter;
		}
	}

	timer_stop(&timer);
	long elapsed = timer_elapsed_ms(&timer);

	// Build JSON
	size_t approx_size = total * 6 + 256;
	char *json = malloc(approx_size);
	if (!json) { free(iters); return strdup("{\"error\":\"Memory allocation failed\"}"); }

	int pos = snprintf(json, approx_size, "{\"width\":%d,\"height\":%d,\"max_iter\":%d,\"elapsed_ms\":%ld,\"data\":[",
					   width, height, max_iter, elapsed);
	for (long i = 0; i < total; i++) {
		if (i) pos += snprintf(json + pos, approx_size - pos, ",%d", iters[i]);
		else pos += snprintf(json + pos, approx_size - pos, "%d", iters[i]);
	}
	snprintf(json + pos, approx_size - pos, "]}");

	free(iters);
	return json;
}