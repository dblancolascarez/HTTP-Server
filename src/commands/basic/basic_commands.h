#ifndef BASIC_COMMANDS_H
#define BASIC_COMMANDS_H

// Function declaration for fibonacci command
char* handle_fibonacci(const char* n_str);

// Function declaration for hash command
char* handle_hash(const char* text);

// Function declaration for random command
char* handle_random(const char* count_str, const char* min_str, const char* max_str);

// Reverses a string and returns the result in a JSON format
// The caller is responsible for freeing the returned string
char* handle_reverse(const char* text);

#endif // BASIC_COMMANDS_H