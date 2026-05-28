#ifndef ZENITH_SCHEDULER_H
#define ZENITH_SCHEDULER_H

#include "zenith/compiler.h"
#include "zenith/interrupt_frame.h"
#include "zenith/vmm.h"

#include <stdint.h>

typedef void (*kernel_thread_entry)(void *arg);

void scheduler_init(void);
uint64_t scheduler_create_kernel_thread(const char *name, kernel_thread_entry entry, void *arg);
uint64_t scheduler_create_user_process(const char *name,
                                       struct vmm_address_space address_space,
                                       uint64_t entry,
                                       uint64_t user_stack_top,
                                       uint64_t arg0,
                                       uint64_t arg1,
                                       uint64_t parent_id);
struct interrupt_frame *scheduler_on_timer_interrupt(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_sleep_current_from_frame(struct interrupt_frame *frame, uint64_t ticks);
struct interrupt_frame *scheduler_exit_current_from_frame(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_exit_current_with_status_from_frame(struct interrupt_frame *frame,
                                                                      uint64_t status);
struct interrupt_frame *scheduler_wait_child_from_frame(struct interrupt_frame *frame, uint64_t child_id);
struct interrupt_frame *scheduler_exec_current_from_frame(struct interrupt_frame *frame,
                                                          const char *name,
                                                          struct vmm_address_space address_space,
                                                          uint64_t entry,
                                                          uint64_t user_stack_top,
                                                          uint64_t arg0,
                                                          uint64_t arg1);
uint64_t scheduler_ticks(void);
uint64_t scheduler_current_task_id(void);
uint64_t scheduler_current_parent_id(void);
const char *scheduler_current_task_name(void);
struct vmm_address_space scheduler_current_address_space(void);
void scheduler_block_current(uint64_t wait_channel);
uint64_t scheduler_blocked_count(uint64_t wait_channel);
uint64_t scheduler_wake_one(uint64_t wait_channel);
uint64_t scheduler_wake_all(uint64_t wait_channel);
void scheduler_start_thread_self_test(void);
void scheduler_self_test(void);
ZENITH_NORETURN void scheduler_exit_current_thread(void);

#endif
