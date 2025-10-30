#ifndef FILES_COMMANDS_H
#define FILES_COMMANDS_H

// Function declaration for createfile command
char* handle_createfile(const char* name_str, const char* content_str, const char* repeat_str);

// Function declaration for deletefile command
char* handle_deletefile(const char* name_str);

#endif //FILES_COMMANDS_H