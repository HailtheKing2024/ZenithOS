#include "zenith/ipc.h"

#include "zenith/panic.h"
#include "zenith/serial.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    IPC_MAX_PORTS = 16,
    IPC_QUEUE_LENGTH = 8
};

struct ipc_port {
    bool allocated;
    uint64_t id;
    uint64_t owner;
    uint64_t head;
    uint64_t tail;
    uint64_t count;
    struct ipc_message queue[IPC_QUEUE_LENGTH];
};

static struct ipc_port ports[IPC_MAX_PORTS];
static uint64_t next_port_id = 1;

static struct ipc_port *find_port(uint64_t port_id)
{
    for (size_t i = 0; i < IPC_MAX_PORTS; i++) {
        if (ports[i].allocated && ports[i].id == port_id) {
            return &ports[i];
        }
    }

    return NULL;
}

void ipc_init(void)
{
    for (size_t i = 0; i < IPC_MAX_PORTS; i++) {
        ports[i].allocated = false;
        ports[i].id = 0;
        ports[i].owner = 0;
        ports[i].head = 0;
        ports[i].tail = 0;
        ports[i].count = 0;
    }

    next_port_id = 1;
    serial_write("[ipc] fixed port table initialized\n");
}

enum ipc_result ipc_create_port(uint64_t owner, uint64_t *port_id_out)
{
    if (port_id_out == NULL) {
        return IPC_BAD_MESSAGE;
    }

    for (size_t i = 0; i < IPC_MAX_PORTS; i++) {
        if (!ports[i].allocated) {
            ports[i].allocated = true;
            ports[i].id = next_port_id++;
            ports[i].owner = owner;
            ports[i].head = 0;
            ports[i].tail = 0;
            ports[i].count = 0;
            *port_id_out = ports[i].id;
            return IPC_OK;
        }
    }

    return IPC_TABLE_FULL;
}

enum ipc_result ipc_send(uint64_t port_id, const struct ipc_message *message)
{
    if (message == NULL || message->word_count > IPC_MAX_MESSAGE_WORDS) {
        return IPC_BAD_MESSAGE;
    }

    struct ipc_port *port = find_port(port_id);
    if (port == NULL) {
        return IPC_BAD_PORT;
    }

    if (port->count == IPC_QUEUE_LENGTH) {
        return IPC_QUEUE_FULL;
    }

    port->queue[port->tail] = *message;
    port->tail = (port->tail + 1) % IPC_QUEUE_LENGTH;
    port->count++;
    return IPC_OK;
}

enum ipc_result ipc_receive(uint64_t port_id, struct ipc_message *message_out)
{
    if (message_out == NULL) {
        return IPC_BAD_MESSAGE;
    }

    struct ipc_port *port = find_port(port_id);
    if (port == NULL) {
        return IPC_BAD_PORT;
    }

    if (port->count == 0) {
        return IPC_WOULD_BLOCK;
    }

    *message_out = port->queue[port->head];
    port->head = (port->head + 1) % IPC_QUEUE_LENGTH;
    port->count--;
    return IPC_OK;
}

void ipc_self_test(void)
{
    uint64_t port_id = 0;
    if (ipc_create_port(1, &port_id) != IPC_OK) {
        panic("ipc self-test could not create port");
    }

    const struct ipc_message outbound = {
        .sender = 7,
        .tag = 0x51,
        .word_count = 2,
        .words = {0x111, 0x222, 0, 0}
    };

    if (ipc_send(port_id, &outbound) != IPC_OK) {
        panic("ipc self-test send failed");
    }

    struct ipc_message inbound = {0};
    if (ipc_receive(port_id, &inbound) != IPC_OK) {
        panic("ipc self-test receive failed");
    }

    if (inbound.sender != outbound.sender ||
        inbound.tag != outbound.tag ||
        inbound.word_count != outbound.word_count ||
        inbound.words[0] != outbound.words[0] ||
        inbound.words[1] != outbound.words[1]) {
        panic("ipc self-test message mismatch");
    }

    if (ipc_receive(port_id, &inbound) != IPC_WOULD_BLOCK) {
        panic("ipc self-test empty receive mismatch");
    }

    serial_write("[ipc] self-test ok port=");
    serial_write_u64_dec(port_id);
    serial_write("\n");
}
