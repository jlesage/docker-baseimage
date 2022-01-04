#ifndef __CINIT_UTILS_H__
#define __CINIT_UTILS_H__

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define DIM(a) (sizeof(a)/sizeof(a[0]))

/**
 * Enumeration to identify a standard output stream.
 */
typedef enum {
    STDOUT = 0, /**< The standard output. */
    STDERR,     /**< The standard error output. */
} std_output_t;

typedef void (*exec_cmd_line_callback_t)(const char *line, std_output_t src, void *data);

typedef void (*line_callback_t)(int fd, const char *line, void *data);

/**
 * Execute a command and wait for its completion.
 *
 * @param[in] silent Whether or not the output (stdout and stderr) of the
 *                   command should be discarded.
 * @param[in] output_prefix Optional prefix to apply to the output (stdout and
 *                          stderr) of the command.
 * @param[in] cmd Path to program to execute.
 * @param[in] ... Argument(s) of the command.
 * 
 * @return Command's exit code or -1 on error.
 */
int exec_cmd(bool silent, const char *output_prefix, const char *cmd, ...);

/**
 * Execute a command and wait for its completion.
 *
 * param[in,out] buf Pointer to the buffer where to store the command's output.
 *                   If buffer is NULL, memory will be dynamically allocated.
 * param[in,out] bufsize Size of the provided buffer.
 * @param[in] cmd Path to program to execute.
 * @param[in] ... Argument(s) of the command.
 *
 * @return Command's exit code or -1 on error.
 */
#if 0
int exec_cmd_with_output(char **buf, size_t *bufsize, const char *cmd, ...);
#endif

/**
 * Execute a command and wait for its completion while getting informed of each
 * line from its output.
 *
 * @param[in] callback Function to invoke for each output line of the command.
 * @param[in] callback_data Custom data to be passed to the callback.
 * @param[in] cmd Path to program to execute.
 * @param[in] ... Argument(s) of the command.
 *
 * @return Command's exit code or -1 on error.
 */
int exec_cmd_with_line_callback(exec_cmd_line_callback_t callback, void *callback_data, const char *cmd, ...);

/**
 * Remove the leading and trailing character from a string.
 *
 * @param[in] s Pointer to the string.
 * @param[in] c Character to trim.
 *
 * @return Pointer to the trimmed string.
 */
char *trim_char(char *s, char c);

/**
 * Remove leading and trailing spaces from a string.
 *
 * @param[in] s Pointer to the string.
 *
 * @return Pointer to the trimmed string.
 */
char *trim(char *s);

/**
 * Remove consecutive duplicates of character from a string.
 *
 * For example, removing consecutive duplicates of 'a' from 'aaabbb' result in
 * 'abbb'.
 *
 * @param[in] s Pointer to the string.
 * @param[in] c Character to remove.
 */
void remove_duplicated_char(char *s, char c);

/**
 * Remove all occurence of a character from a string.
 *
 * @param[in] s Pointer to the string.
 * @param[in] c Character to remove.
 */
void remove_all_char(char *s, char c);

/**
 * Terminate a string at the first EOL character.
 *
 * @param[in] s Pointer to the string.
 */
void terminate_at_first_eol(char *s);

/**
 * Split a string.
 * 
 * @param[in] buf Input string to be splited.
 * @param[in] c Delimiter.
 * @param[out] len Number of elements in the returned array.
 * @param[in] plus Number of additional elements to add to the array.
 * @param[in] ofs Offset at which the first element should be placed in the
 *            array.
 *
 * @return Array of pointers to string elements.
 */
char **split(char *buf, int c, size_t *len, int plus, int ofs);

/**
 * Read from file descriptors and invoke the specified callback for each line.
 *
 * @param[in] fds Table of file descriptors to read from.
 * @param[in] num_fds Number of file descriptors int the table.
 * @param[in] callback Function to be invoked.
 * @param[in] callback_data Custom data to be passed to the callback function.
 *
 * @return 0 on success, -1 if error occurred.
 */
int read_lines(int *fds, size_t num_fds, line_callback_t callback, void *callback_data);

/**
 * Store the content of a text file into the provided buffer.
 *
 * On success, the buffer is guaranteed to be NULL terminated.
 *
 * @param[in] filepath Path to the file to read.
 * param[in,out] buf Pointer to the buffer where to store the file's content.
 *                   When pointing to a NULL buffer, memory is dynamically
 *                   allocated.
 * param[in] bufsize Size of the provided buffer.
 */
void read_file(const char *filepath, char **buf, size_t bufsize);

/**
 * Convert a string to a boolean value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_bool(const char *str, bool *result);

/**
 * Convert a string to a signed integer value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_int(const char *str, int *result);

/**
 * Convert a string to a unsigned integer value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_uint(const char *str, unsigned int *result);

/**
 * Convert a string to an interval value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_interval(const char *str, unsigned int *result);

/**
 * Convert a string to a Linux UID value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_uid(const char *str, uid_t *result);

/**
 * Convert a string to a Linux group ID value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_gid(const char *str, gid_t *result);

/**
 * Convert a string to a Unix mode value.
 *
 * @param[in] str Input string to convert.
 * @param[out] result Where to store the converted value.
 */
void string_to_mode(const char *str, mode_t *result);

/**
 * Load configuration item as a string value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] buf Where the result will be stored.  Memory is dynamically
 *                 allocated when pointing to a NULL buffer.
 * @param[in] bufsize Size of the buffer.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_string(const char *filepath, char **buf, size_t bufsize);

/**
 * Load configuration item as a boolean value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_bool(const char *filepath, bool *result);

/**
 * Load configuration item as a signed integer value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_int(const char *filepath, int *result);

/**
 * Load configuration item as an unsigned integer value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_uint(const char *filepath, unsigned int *result);

/**
 * Load configuration item as an interval value.
 *
 * The value can be an unsigned integer or one of the following keywords:
 * yearly, monthly, weekly, daily, hourly.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_interval(const char *filepath, unsigned int *result);

/**
 * Load configuration item as a Liux user ID value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_uid(const char *filepath, uid_t *result);

/**
 * Load configuration item as a Linux group ID value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_gid(const char *filepath, gid_t *result);

/**
 * Load configuration item as a Unix mode value.
 *
 * @param[in] filepath Path to the configuration item file to load.
 * @param[out] result Where the result will be stored.
 *
 * @return true if the value was loaded, false if value was not set.
 */
bool load_value_as_mode(const char *filepath, mode_t *result);

#endif // __CINIT_UTILS_H__
