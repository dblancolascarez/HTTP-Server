#ifndef CPU_BOUND_COMMANDS_H
#define CPU_BOUND_COMMANDS_H

// Function declaration for isprime command
char* handle_isprime(const char* n_str);

// Function declaration for factor command
char* handle_factor(const char* n_str);

// Function declaration for pi command
char* handle_pi(const char* digits_str);

// Function declaration for mandelbrot command
char* handle_mandelbrot(const char* width_str, const char* height_str, const char* max_iter_str);

// Function declaration for matrix multiplication command
char* handle_matrixmul(const char* size_str, const char* seed_str);

#endif // CPU_BOUND_COMMANDS_H

