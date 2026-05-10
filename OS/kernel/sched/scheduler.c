#include "zenith/scheduler.h"

#include "zenith/cpu.h"
#include "zenith/kheap.h"
#include "zenith/panic.h"
#include "zenith/serial.h"

#include <stdbool.h>
#include <stddef.h>

enum {
    MAX_TASKS = 16,
    KERNEL_THREAD_STACK_SIZE = 16384,
    RFLAGS_INTERRUPT_ENABLE = 0x202,
    BLOCK_SELF_TEST_CHANNEL = 0x5a01,
    TIMER_WAIT_CHANNEL = 0x54494d45,
    PROCESS_WAIT_CHANNEL = 0x50524f43
};

enum task_state {
    TASK_UNUSED,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DONE
};

struct task {
    uint64_t id;
    const char *name;
    enum task_state state;
    uint64_t runtime_ticks;
    struct interrupt_frame *frame;
    uint8_t *stack_base;
    uint64_t kernel_stack_top;
    struct vmm_address_space address_space;
    uint64_t switches;
    uint64_t wait_channel;
    uint64_t wake_tick;
    uint64_t parent_id;
    uint64_t wait_child_id;
    uint64_t exit_status;
    bool reaped;
    bool user;
};

static struct task tasks[MAX_TASKS];
static struct task *current_task;
static uint64_t total_ticks;
static uint64_t next_task_id = 1;
static bool initialized;
static volatile uint64_t thread_a_runs;
static volatile uint64_t thread_b_runs;
static volatile uint64_t block_waiter_started;
static volatile uint64_t block_waiter_resumed;
static volatile uint64_t block_waker_done;

extern void x86_64_thread_entry(void);

static uint16_t read_cs(void)
{
    uint16_t value;
    __asm__ volatile ("movw %%cs, %0" : "=r"(value));
    return value;
}

static uint16_t read_ss(void)
{
    uint16_t value;
    __asm__ volatile ("movw %%ss, %0" : "=r"(value));
    return value;
}

static struct task *find_free_task(void)
{
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_UNUSED || (tasks[i].state == TASK_DONE && tasks[i].reaped)) {
            return &tasks[i];
        }
    }

    return NULL;
}

static struct task *find_child_task(uint64_t parent_id, uint64_t child_id)
{
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_UNUSED &&
            tasks[i].parent_id == parent_id &&
            (child_id == 0 || tasks[i].id == child_id) &&
            !tasks[i].reaped) {
            return &tasks[i];
        }
    }

    return NULL;
}

static struct task *next_ready_task(void)
{
    size_t current_index = 0;

    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (&tasks[i] == current_task) {
            current_index = i;
            break;
        }
    }

    for (size_t step = 1; step <= MAX_TASKS; step++) {
        size_t index = (current_index + step) % MAX_TASKS;
        if (tasks[index].state == TASK_READY) {
            return &tasks[index];
        }
    }

    return NULL;
}

static void activate_task(struct task *task)
{
    x86_64_set_kernel_stack(task->kernel_stack_top);
    vmm_switch_address_space(task->address_space);
}

static struct interrupt_frame *schedule_next_from_current_frame(struct interrupt_frame *frame)
{
    if (current_task != NULL && frame != NULL) {
        current_task->frame = frame;
    }

    struct task *next = next_ready_task();
    if (next == NULL || next->frame == NULL) {
        if (current_task == NULL ||
            current_task->state == TASK_BLOCKED ||
            current_task->state == TASK_DONE) {
            panic("scheduler found no runnable task");
        }

        current_task->state = TASK_RUNNING;
        activate_task(current_task);
        return frame;
    }

    next->state = TASK_RUNNING;
    next->switches++;
    current_task = next;
    activate_task(next);
    return next->frame;
}

static void wake_timer_tasks(void)
{
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            tasks[i].wait_channel == TIMER_WAIT_CHANNEL &&
            tasks[i].wake_tick <= total_ticks) {
            tasks[i].wait_channel = 0;
            tasks[i].wake_tick = 0;
            tasks[i].state = TASK_READY;
        }
    }
}

uint64_t scheduler_create_kernel_thread(const char *name, kernel_thread_entry entry, void *arg)
{
    struct task *task = find_free_task();
    if (task == NULL) {
        panic("scheduler task table exhausted");
    }

    uint8_t *stack = kmalloc_aligned(KERNEL_THREAD_STACK_SIZE, 16);
    uintptr_t aligned_stack_top = ((uintptr_t)stack + KERNEL_THREAD_STACK_SIZE) & ~(uintptr_t)0xf;
    uintptr_t runtime_stack = aligned_stack_top - 24;
    uintptr_t frame_address = runtime_stack - INTERRUPT_FRAME_CORE_SIZE;

    struct interrupt_frame *frame = (struct interrupt_frame *)frame_address;
    *frame = (struct interrupt_frame){
        .rdi = (uint64_t)(uintptr_t)entry,
        .rsi = (uint64_t)(uintptr_t)arg,
        .rip = (uint64_t)(uintptr_t)x86_64_thread_entry,
        .cs = read_cs(),
        .rflags = RFLAGS_INTERRUPT_ENABLE,
        .rsp = runtime_stack,
        .ss = read_ss()
    };

    task->id = next_task_id++;
    task->name = name;
    task->state = TASK_READY;
    task->runtime_ticks = 0;
    task->frame = frame;
    task->stack_base = stack;
    task->kernel_stack_top = aligned_stack_top;
    task->address_space = vmm_kernel_address_space();
    task->switches = 0;
    task->wait_channel = 0;
    task->wake_tick = 0;
    task->parent_id = 0;
    task->wait_child_id = 0;
    task->exit_status = 0;
    task->reaped = false;
    task->user = false;

    serial_write("[sched] created thread id=");
    serial_write_u64_dec(task->id);
    serial_write(" name=");
    serial_write(task->name);
    serial_write(" stack=");
    serial_write_u64_hex((uint64_t)(uintptr_t)stack);
    serial_write("\n");

    return task->id;
}

uint64_t scheduler_create_user_process(const char *name,
                                       struct vmm_address_space address_space,
                                       uint64_t entry,
                                       uint64_t user_stack_top,
                                       uint64_t arg0,
                                       uint64_t arg1,
                                       uint64_t parent_id)
{
    struct task *task = find_free_task();
    if (task == NULL) {
        serial_write("[sched] user process table exhausted\n");
        return 0;
    }

    uint8_t *stack = kmalloc_aligned(KERNEL_THREAD_STACK_SIZE, 16);
    uintptr_t aligned_stack_top = ((uintptr_t)stack + KERNEL_THREAD_STACK_SIZE) & ~(uintptr_t)0xf;
    uintptr_t runtime_stack = aligned_stack_top - 24;
    uintptr_t frame_address = runtime_stack - INTERRUPT_FRAME_CORE_SIZE;

    struct interrupt_frame *frame = (struct interrupt_frame *)frame_address;
    *frame = (struct interrupt_frame){
        .rdi = arg0,
        .rsi = arg1,
        .rip = entry,
        .cs = X86_64_USER_CS,
        .rflags = RFLAGS_INTERRUPT_ENABLE,
        .rsp = user_stack_top,
        .ss = X86_64_USER_DS
    };

    task->id = next_task_id++;
    task->name = name;
    task->state = TASK_READY;
    task->runtime_ticks = 0;
    task->frame = frame;
    task->stack_base = stack;
    task->kernel_stack_top = aligned_stack_top;
    task->address_space = address_space;
    task->switches = 0;
    task->wait_channel = 0;
    task->wake_tick = 0;
    task->parent_id = parent_id;
    task->wait_child_id = 0;
    task->exit_status = 0;
    task->reaped = false;
    task->user = true;

    serial_write("[sched] created user process id=");
    serial_write_u64_dec(task->id);
    serial_write(" name=");
    serial_write(task->name);
    serial_write(" pml4=");
    serial_write_u64_hex(task->address_space.pml4_physical);
    serial_write(" parent=");
    serial_write_u64_dec(task->parent_id);
    serial_write("\n");

    return task->id;
}

static void test_thread_a(void *arg)
{
    (void)arg;

    for (uint64_t i = 0; i < 3; i++) {
        thread_a_runs++;
        for (volatile uint64_t spin = 0; spin < 100000; spin++) {
        }
    }
}

static void test_thread_b(void *arg)
{
    (void)arg;

    for (uint64_t i = 0; i < 3; i++) {
        thread_b_runs++;
        for (volatile uint64_t spin = 0; spin < 100000; spin++) {
        }
    }
}

static void test_block_waiter(void *arg)
{
    (void)arg;

    block_waiter_started = 1;
    scheduler_block_current(BLOCK_SELF_TEST_CHANNEL);
    block_waiter_resumed = 1;
}

static void test_block_waker(void *arg)
{
    (void)arg;

    while (scheduler_blocked_count(BLOCK_SELF_TEST_CHANNEL) == 0) {
        __asm__ volatile ("hlt");
    }

    if (scheduler_wake_one(BLOCK_SELF_TEST_CHANNEL) != 1) {
        panic("scheduler wake-one self-test missed waiter");
    }

    block_waker_done = 1;
}

static bool scheduler_self_test_complete(void)
{
    return thread_a_runs != 0 &&
           thread_b_runs != 0 &&
           block_waiter_started != 0 &&
           block_waiter_resumed != 0 &&
           block_waker_done != 0;
}

void scheduler_init(void)
{
    for (size_t i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_UNUSED;
    }

    tasks[0].id = 0;
    tasks[0].name = "boot";
    tasks[0].state = TASK_RUNNING;
    tasks[0].runtime_ticks = 0;
    tasks[0].frame = NULL;
    tasks[0].stack_base = NULL;
    tasks[0].kernel_stack_top = 0;
    __asm__ volatile ("movq %%rsp, %0" : "=r"(tasks[0].kernel_stack_top));
    tasks[0].kernel_stack_top &= ~(uint64_t)0xf;
    tasks[0].address_space = vmm_kernel_address_space();
    tasks[0].switches = 0;
    tasks[0].wait_channel = 0;
    tasks[0].wake_tick = 0;
    tasks[0].parent_id = 0;
    tasks[0].wait_child_id = 0;
    tasks[0].exit_status = 0;
    tasks[0].reaped = false;
    tasks[0].user = false;

    current_task = &tasks[0];
    total_ticks = 0;
    initialized = true;

    serial_write("[sched] initialized current=");
    serial_write(current_task->name);
    serial_write("\n");
}

void scheduler_start_thread_self_test(void)
{
    if (!initialized) {
        panic("scheduler self-test threads before init");
    }

    thread_a_runs = 0;
    thread_b_runs = 0;
    block_waiter_started = 0;
    block_waiter_resumed = 0;
    block_waker_done = 0;
    scheduler_create_kernel_thread("test-a", test_thread_a, NULL);
    scheduler_create_kernel_thread("test-b", test_thread_b, NULL);
    scheduler_create_kernel_thread("block-wait", test_block_waiter, NULL);
    scheduler_create_kernel_thread("block-wake", test_block_waker, NULL);
}

struct interrupt_frame *scheduler_on_timer_interrupt(struct interrupt_frame *frame)
{
    if (!initialized || current_task == NULL) {
        return frame;
    }

    total_ticks++;
    wake_timer_tasks();

    if (total_ticks <= 4) {
        serial_write("[sched] irq tick=");
        serial_write_u64_dec(total_ticks);
        serial_write(" current=");
        serial_write(current_task->name);
        serial_write("\n");
    }

    if (current_task->state == TASK_RUNNING) {
        current_task->runtime_ticks++;
        current_task->frame = frame;
        current_task->state = TASK_READY;
    } else if (current_task->state == TASK_BLOCKED) {
        current_task->frame = frame;
    }

    struct task *previous = current_task;
    struct interrupt_frame *next_frame = schedule_next_from_current_frame(frame);
    if (total_ticks <= 4) {
        if (previous != current_task) {
            serial_write("[sched] switch to=");
            serial_write(current_task->name);
            serial_write("\n");
        }
    }
    return next_frame;
}

struct interrupt_frame *scheduler_sleep_current_from_frame(struct interrupt_frame *frame, uint64_t ticks)
{
    if (!initialized || current_task == NULL || current_task == &tasks[0]) {
        panic("scheduler cannot sleep current task");
    }

    if (ticks == 0) {
        ticks = 1;
    }

    current_task->runtime_ticks++;
    current_task->frame = frame;
    current_task->wait_channel = TIMER_WAIT_CHANNEL;
    current_task->wake_tick = total_ticks + ticks;
    current_task->state = TASK_BLOCKED;
    return schedule_next_from_current_frame(frame);
}

struct interrupt_frame *scheduler_exit_current_from_frame(struct interrupt_frame *frame)
{
    return scheduler_exit_current_with_status_from_frame(frame, 0);
}

static void wake_process_waiter(struct task *child)
{
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            tasks[i].wait_channel == PROCESS_WAIT_CHANNEL &&
            tasks[i].id == child->parent_id &&
            (tasks[i].wait_child_id == 0 || tasks[i].wait_child_id == child->id)) {
            if (tasks[i].frame != NULL) {
                tasks[i].frame->rax = child->exit_status;
            }
            tasks[i].wait_channel = 0;
            tasks[i].wait_child_id = 0;
            tasks[i].wake_tick = 0;
            tasks[i].state = TASK_READY;
            child->reaped = true;
            return;
        }
    }
}

struct interrupt_frame *scheduler_exit_current_with_status_from_frame(struct interrupt_frame *frame,
                                                                      uint64_t status)
{
    if (!initialized || current_task == NULL || current_task == &tasks[0]) {
        panic("scheduler cannot exit current task");
    }

    current_task->frame = frame;
    current_task->state = TASK_DONE;
    current_task->wait_channel = 0;
    current_task->wait_child_id = 0;
    current_task->wake_tick = 0;
    current_task->exit_status = status;
    wake_process_waiter(current_task);

    serial_write("[sched] thread exit name=");
    serial_write(current_task->name);
    serial_write(" status=");
    serial_write_u64_dec(status);
    serial_write("\n");

    return schedule_next_from_current_frame(frame);
}

struct interrupt_frame *scheduler_wait_child_from_frame(struct interrupt_frame *frame, uint64_t child_id)
{
    if (!initialized || current_task == NULL || current_task == &tasks[0]) {
        panic("scheduler cannot wait from current task");
    }

    struct task *child = find_child_task(current_task->id, child_id);
    if (child == NULL) {
        frame->rax = UINT64_MAX;
        return frame;
    }

    if (child->state == TASK_DONE) {
        frame->rax = child->exit_status;
        child->reaped = true;
        return frame;
    }

    current_task->runtime_ticks++;
    current_task->frame = frame;
    current_task->wait_channel = PROCESS_WAIT_CHANNEL;
    current_task->wait_child_id = child_id;
    current_task->wake_tick = 0;
    current_task->state = TASK_BLOCKED;
    return schedule_next_from_current_frame(frame);
}

struct interrupt_frame *scheduler_exec_current_from_frame(struct interrupt_frame *frame,
                                                          const char *name,
                                                          struct vmm_address_space address_space,
                                                          uint64_t entry,
                                                          uint64_t user_stack_top,
                                                          uint64_t arg0,
                                                          uint64_t arg1)
{
    if (!initialized || current_task == NULL || current_task == &tasks[0]) {
        panic("scheduler cannot exec current task");
    }

    current_task->name = name;
    current_task->address_space = address_space;
    current_task->user = true;

    *frame = (struct interrupt_frame){
        .rdi = arg0,
        .rsi = arg1,
        .rip = entry,
        .cs = X86_64_USER_CS,
        .rflags = RFLAGS_INTERRUPT_ENABLE,
        .rsp = user_stack_top,
        .ss = X86_64_USER_DS
    };
    current_task->frame = frame;
    activate_task(current_task);
    return frame;
}

uint64_t scheduler_ticks(void)
{
    return total_ticks;
}

uint64_t scheduler_current_task_id(void)
{
    if (current_task == NULL) {
        return 0;
    }

    return current_task->id;
}

const char *scheduler_current_task_name(void)
{
    if (current_task == NULL) {
        return "none";
    }

    return current_task->name;
}

struct vmm_address_space scheduler_current_address_space(void)
{
    if (current_task == NULL) {
        return vmm_kernel_address_space();
    }

    return current_task->address_space;
}

void scheduler_block_current(uint64_t wait_channel)
{
    if (!initialized || current_task == NULL || current_task == &tasks[0]) {
        panic("scheduler cannot block current task");
    }

    __asm__ volatile ("cli" : : : "memory");
    current_task->wait_channel = wait_channel;
    current_task->state = TASK_BLOCKED;
    __asm__ volatile ("sti" : : : "memory");

    for (;;) {
        __asm__ volatile ("hlt");
        if (current_task->state == TASK_RUNNING && current_task->wait_channel == 0) {
            return;
        }
    }
}

uint64_t scheduler_blocked_count(uint64_t wait_channel)
{
    uint64_t count = 0;

    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == wait_channel) {
            count++;
        }
    }

    return count;
}

uint64_t scheduler_wake_one(uint64_t wait_channel)
{
    __asm__ volatile ("cli" : : : "memory");

    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == wait_channel) {
            tasks[i].wait_channel = 0;
            tasks[i].wake_tick = 0;
            tasks[i].state = TASK_READY;
            __asm__ volatile ("sti" : : : "memory");
            return 1;
        }
    }

    __asm__ volatile ("sti" : : : "memory");
    return 0;
}

uint64_t scheduler_wake_all(uint64_t wait_channel)
{
    uint64_t count = 0;

    __asm__ volatile ("cli" : : : "memory");

    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_BLOCKED && tasks[i].wait_channel == wait_channel) {
            tasks[i].wait_channel = 0;
            tasks[i].wake_tick = 0;
            tasks[i].state = TASK_READY;
            count++;
        }
    }

    __asm__ volatile ("sti" : : : "memory");
    return count;
}

ZENITH_NORETURN void scheduler_exit_current_thread(void)
{
    __asm__ volatile ("cli");

    if (current_task != NULL) {
        serial_write("[sched] thread exit name=");
        serial_write(current_task->name);
        serial_write("\n");
        current_task->state = TASK_DONE;
        current_task->wait_channel = 0;
    }

    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void scheduler_self_test(void)
{
    if (!initialized || tasks[0].runtime_ticks == 0) {
        panic("scheduler boot task did not receive timer ticks");
    }

    uint64_t deadline = total_ticks + 64;
    while (!scheduler_self_test_complete() && total_ticks < deadline) {
        __asm__ volatile ("hlt");
    }

    if (thread_a_runs == 0 || thread_b_runs == 0) {
        panic("scheduler thread self-test did not run both threads");
    }

    if (block_waiter_started == 0 || block_waiter_resumed == 0 || block_waker_done == 0) {
        serial_write("[sched] block self-test flags started=");
        serial_write_u64_dec(block_waiter_started);
        serial_write(" resumed=");
        serial_write_u64_dec(block_waiter_resumed);
        serial_write(" waker=");
        serial_write_u64_dec(block_waker_done);
        serial_write(" ticks=");
        serial_write_u64_dec(total_ticks);
        serial_write("\n");
        panic("scheduler block/wakeup self-test failed");
    }

    serial_write("[sched] self-test ok task=");
    serial_write(scheduler_current_task_name());
    serial_write(" ticks=");
    serial_write_u64_dec(scheduler_ticks());
    serial_write(" a_runs=");
    serial_write_u64_dec(thread_a_runs);
    serial_write(" b_runs=");
    serial_write_u64_dec(thread_b_runs);
    serial_write(" block_resume=");
    serial_write_u64_dec(block_waiter_resumed);
    serial_write("\n");
}
