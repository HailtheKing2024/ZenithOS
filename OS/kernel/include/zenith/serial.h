#ifndef ZENITH_SERIAL_H
#define ZENITH_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

void serial_init(void);
void serial_write_char(char value);
void serial_write(const char *message);
void serial_write_u64_hex(uint64_t value);
void serial_write_u64_dec(uint64_t value);
bool serial_try_read_char(char *value_out);

#endif
