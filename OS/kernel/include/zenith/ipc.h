#ifndef ZENITH_IPC_H
#define ZENITH_IPC_H

#include <stdint.h>

enum {
    IPC_MAX_MESSAGE_WORDS = 4
};

enum ipc_result {
    IPC_OK = 0,
    IPC_BAD_PORT = 1,
    IPC_QUEUE_FULL = 2,
    IPC_WOULD_BLOCK = 3,
    IPC_BAD_MESSAGE = 4,
    IPC_TABLE_FULL = 5
};

struct ipc_message {
    uint64_t sender;
    uint64_t tag;
    uint64_t word_count;
    uint64_t words[IPC_MAX_MESSAGE_WORDS];
};

void ipc_init(void);
enum ipc_result ipc_create_port(uint64_t owner, uint64_t *port_id_out);
enum ipc_result ipc_send(uint64_t port_id, const struct ipc_message *message);
enum ipc_result ipc_receive(uint64_t port_id, struct ipc_message *message_out);
void ipc_self_test(void);

#endif
