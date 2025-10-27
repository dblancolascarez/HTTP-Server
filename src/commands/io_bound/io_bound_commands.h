#ifndef IO_BOUND_COMMANDS_H
#define IO_BOUND_COMMANDS_H

// Function declaration for sortfile command
char* handle_sortfile(const char* name_str, const char* algo_str);

// Function declaration for wordcount command
char* handle_wordcount(const char* name_str);

// Function declaration for hashfile command
char* handle_hashfile(const char* name_str, const char* algo_str);

#endif //IO_BOUND_COMMANDS_H

