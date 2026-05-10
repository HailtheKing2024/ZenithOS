#ifndef ZENITH_CONSOLE_H
#define ZENITH_CONSOLE_H

#include <stdint.h>

#define CONSOLE_ACTION_NONE UINT64_C(0)
#define CONSOLE_ACTION_LINE_READY UINT64_C(1)

void console_init(void);
uint64_t console_handle_key(uint64_t code);
void console_write_line(const char *text);
void console_render_prompt(void);
uint64_t console_read_line(char *dest, uint64_t dest_size);
void console_clear(void);
uint64_t console_copy_argument(char *dest, uint64_t dest_size);

#endif
