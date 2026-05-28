#include "zenith/user.h"

#include <stdbool.h>

enum {
    TERM_LINES = 10,
    TERM_CAP = Z_UI_TEXT_CAP,
    IO_CAP = 512,
    ARG_MAX = 6,
    HISTORY_MAX = 8,
    ENV_MAX = 4,
    ALIAS_MAX = 4,
    NAME_CAP = 16,
    VALUE_CAP = 40,
    CLASS_NATIVE = 1,
    CLASS_BRIDGE_ONLY = 2
};

struct parsed_command {
    uint64_t argc;
    char *argv[ARG_MAX];
};

struct env_entry {
    bool used;
    char key[NAME_CAP];
    char value[VALUE_CAP];
};

struct alias_entry {
    bool used;
    char name[NAME_CAP];
    char value[VALUE_CAP];
};

struct command;
typedef void (*command_handler)(const struct command *command, const struct parsed_command *parsed);

struct command {
    const char *name;
    uint64_t classification;
    uint64_t min_args;
    uint64_t max_args;
    command_handler handler;
    const char *help;
};

static char lines[TERM_LINES][TERM_CAP];
static char input[TERM_CAP];
static char parse_buffer[TERM_CAP];
static char bridge_buffer[TERM_CAP];
static char history[HISTORY_MAX][TERM_CAP];
static char cwd[VALUE_CAP];
static struct env_entry env_table[ENV_MAX];
static struct alias_entry alias_table[ALIAS_MAX];
static uint64_t line_count;
static uint64_t input_len;
static uint64_t cursor_pos;
static uint64_t history_count;
static uint64_t history_next;
static uint64_t history_view;
static uint64_t esc_state;

static void handle_help(const struct command *command, const struct parsed_command *parsed);
static void handle_sysinfo(const struct command *command, const struct parsed_command *parsed);
static void handle_echo(const struct command *command, const struct parsed_command *parsed);
static void handle_clear(const struct command *command, const struct parsed_command *parsed);
static void handle_desktop(const struct command *command, const struct parsed_command *parsed);
static void handle_reboot(const struct command *command, const struct parsed_command *parsed);
static void handle_env(const struct command *command, const struct parsed_command *parsed);
static void handle_export(const struct command *command, const struct parsed_command *parsed);
static void handle_alias(const struct command *command, const struct parsed_command *parsed);
static void handle_history(const struct command *command, const struct parsed_command *parsed);
static void handle_whoami(const struct command *command, const struct parsed_command *parsed);
static void handle_pwd(const struct command *command, const struct parsed_command *parsed);
static void handle_cd(const struct command *command, const struct parsed_command *parsed);
static void handle_ls(const struct command *command, const struct parsed_command *parsed);
static void handle_cat(const struct command *command, const struct parsed_command *parsed);
static void handle_exit(const struct command *command, const struct parsed_command *parsed);
static void handle_bridge(const struct command *command, const struct parsed_command *parsed);

static const struct command commands[] = {
    {"HELP", CLASS_NATIVE, 0, 1, handle_help, "HELP [COMMAND]"},
    {"SYSINFO", CLASS_NATIVE, 0, 0, handle_sysinfo, "SYSINFO"},
    {"ECHO", CLASS_NATIVE, 0, ARG_MAX - 1, handle_echo, "ECHO [TEXT]"},
    {"CLEAR", CLASS_NATIVE, 0, 0, handle_clear, "CLEAR"},
    {"DESKTOP", CLASS_NATIVE, 0, 0, handle_desktop, "DESKTOP"},
    {"REBOOT", CLASS_NATIVE, 0, 0, handle_reboot, "REBOOT"},
    {"ENV", CLASS_NATIVE, 0, 0, handle_env, "ENV"},
    {"EXPORT", CLASS_NATIVE, 1, 1, handle_export, "EXPORT KEY=VALUE"},
    {"ALIAS", CLASS_NATIVE, 0, 1, handle_alias, "ALIAS NAME=VALUE"},
    {"HISTORY", CLASS_NATIVE, 0, 0, handle_history, "HISTORY"},
    {"WHOAMI", CLASS_NATIVE, 0, 0, handle_whoami, "WHOAMI"},
    {"PWD", CLASS_NATIVE, 0, 0, handle_pwd, "PWD"},
    {"CD", CLASS_NATIVE, 0, 1, handle_cd, "CD [PATH]"},
    {"LS", CLASS_NATIVE, 0, 1, handle_ls, "LS [PATH]"},
    {"CAT", CLASS_NATIVE, 1, 1, handle_cat, "CAT PATH"},
    {"EXIT", CLASS_NATIVE, 0, 0, handle_exit, "EXIT"},
    {"SETTINGS", CLASS_BRIDGE_ONLY, 0, 0, handle_bridge, "SETTINGS"},
    {"OVERVIEW", CLASS_BRIDGE_ONLY, 0, 0, handle_bridge, "OVERVIEW"},
    {"PKG_INSTALL", CLASS_BRIDGE_ONLY, 1, 1, handle_bridge, "PKG_INSTALL NAME"},
    {"PKG_REMOVE", CLASS_BRIDGE_ONLY, 1, 1, handle_bridge, "PKG_REMOVE NAME"},
    {"PKG_UPDATE", CLASS_BRIDGE_ONLY, 0, 0, handle_bridge, "PKG_UPDATE"},
    {"PKG_SEARCH", CLASS_BRIDGE_ONLY, 1, 1, handle_bridge, "PKG_SEARCH QUERY"},
    {"PKG_LIST", CLASS_BRIDGE_ONLY, 0, 0, handle_bridge, "PKG_LIST"},
    {"PKG_INFO", CLASS_BRIDGE_ONLY, 1, 1, handle_bridge, "PKG_INFO NAME"}
};

static const uint64_t command_count = sizeof(commands) / sizeof(commands[0]);

static char to_upper(char value)
{
    if (value >= 'a' && value <= 'z') {
        return (char)(value - 'a' + 'A');
    }

    return value;
}

static char to_lower(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return (char)(value - 'A' + 'a');
    }

    return value;
}

static bool is_space(char value)
{
    return value == ' ' || value == '\t';
}

static bool string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left++ != *right++) {
            return false;
        }
    }

    return *left == '\0' && *right == '\0';
}

static bool string_starts_with(const char *value, const char *prefix)
{
    while (*prefix != '\0') {
        if (*value++ != *prefix++) {
            return false;
        }
    }

    return true;
}

static uint64_t string_length(const char *value)
{
    uint64_t length = 0;
    while (value[length] != '\0') {
        length++;
    }
    return length;
}

static void copy_text(char *dest, uint64_t dest_size, const char *src)
{
    uint64_t index = 0;

    if (dest_size == 0) {
        return;
    }

    while (index + 1 < dest_size && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }

    dest[index] = '\0';
}

static void append_char(char *dest, uint64_t dest_size, char value)
{
    uint64_t length = string_length(dest);
    if (length + 1 >= dest_size) {
        return;
    }

    dest[length] = value;
    dest[length + 1] = '\0';
}

static void append_text(char *dest, uint64_t dest_size, const char *src)
{
    uint64_t length = string_length(dest);
    uint64_t index = 0;

    while (length + 1 < dest_size && src[index] != '\0') {
        dest[length++] = src[index++];
    }

    dest[length] = '\0';
}

static void uppercase_text(char *value)
{
    for (uint64_t i = 0; value[i] != '\0'; i++) {
        value[i] = to_upper(value[i]);
    }
}

static void lowercase_text(char *value)
{
    for (uint64_t i = 0; value[i] != '\0'; i++) {
        value[i] = to_lower(value[i]);
    }
}

static void compose_prompt(char *dest, uint64_t dest_size)
{
    const char prefix[] = "ZENITH> ";
    uint64_t index = 0;

    while (index + 1 < dest_size && prefix[index] != '\0') {
        dest[index] = prefix[index];
        index++;
    }

    for (uint64_t i = 0; index + 1 < dest_size && input[i] != '\0'; i++) {
        if (i == cursor_pos && index + 1 < dest_size) {
            dest[index++] = '|';
        }
        if (index + 1 < dest_size) {
            dest[index++] = input[i];
        }
    }

    if (cursor_pos == input_len && index + 1 < dest_size) {
        dest[index++] = '|';
    }

    dest[index] = '\0';
}

static void push_line(const char *text)
{
    if (line_count < TERM_LINES - 1) {
        copy_text(lines[line_count], TERM_CAP, text);
        line_count++;
        return;
    }

    for (uint64_t row = 1; row < TERM_LINES - 1; row++) {
        copy_text(lines[row - 1], TERM_CAP, lines[row]);
    }

    copy_text(lines[TERM_LINES - 2], TERM_CAP, text);
}

static void push_pair(const char *left, const char *right)
{
    char line[TERM_CAP];
    copy_text(line, sizeof(line), left);
    append_text(line, sizeof(line), right);
    push_line(line);
}

static void redraw(void)
{
    char prompt[TERM_CAP];

    (void)z_ui_send(Z_UI_MSG_CLEAR, 0, 0, 0, 0, 0);
    for (uint64_t row = 0; row < line_count; row++) {
        (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, row, 0, 0, lines[row]);
    }

    compose_prompt(prompt, sizeof(prompt));
    (void)z_ui_send(Z_UI_MSG_SET_LINE, 0, TERM_LINES - 1, 0, 0, prompt);
}

static void reset_input(void)
{
    input_len = 0;
    cursor_pos = 0;
    input[0] = '\0';
    history_view = history_count;
}

static void clear_terminal(void)
{
    line_count = 0;
    reset_input();
    for (uint64_t row = 0; row < TERM_LINES; row++) {
        lines[row][0] = '\0';
    }
}

static void history_add(const char *line)
{
    if (line[0] == '\0') {
        return;
    }

    copy_text(history[history_next], TERM_CAP, line);
    history_next = (history_next + 1) % HISTORY_MAX;
    if (history_count < HISTORY_MAX) {
        history_count++;
    }
    history_view = history_count;
}

static void request_history_load(void)
{
    const char request[] = "BRG HIST_LOAD";

    (void)z_ui_send(Z_UI_MSG_COMMAND,
                    0,
                    Z_UI_CMD_BRIDGE_DATA,
                    0,
                    sizeof(request) - 1,
                    request);
}

static void persist_history_line(const char *line)
{
    char payload[TERM_CAP];

    if (line[0] == '\0') {
        return;
    }

    copy_text(payload, sizeof(payload), "BRG HIST_PUSH ");
    append_text(payload, sizeof(payload), line);
    (void)z_ui_send(Z_UI_MSG_COMMAND,
                    0,
                    Z_UI_CMD_BRIDGE_DATA,
                    0,
                    string_length(payload),
                    payload);
}

static uint64_t history_slot(uint64_t view)
{
    uint64_t first = history_count < HISTORY_MAX ? 0 : history_next;
    return (first + view) % HISTORY_MAX;
}

static void history_load(uint64_t view)
{
    if (view >= history_count) {
        reset_input();
        history_view = history_count;
        return;
    }

    copy_text(input, TERM_CAP, history[history_slot(view)]);
    input_len = string_length(input);
    cursor_pos = input_len;
    history_view = view;
}

static void history_previous(void)
{
    if (history_count == 0) {
        return;
    }

    if (history_view == 0) {
        history_load(0);
    } else {
        history_load(history_view - 1);
    }
}

static void history_next_line(void)
{
    if (history_count == 0) {
        return;
    }

    if (history_view + 1 >= history_count) {
        reset_input();
    } else {
        history_load(history_view + 1);
    }
}

static uint64_t tokenize(char *line, struct parsed_command *parsed)
{
    parsed->argc = 0;
    for (uint64_t i = 0; i < ARG_MAX; i++) {
        parsed->argv[i] = 0;
    }

    uint64_t index = 0;
    while (line[index] != '\0') {
        while (is_space(line[index])) {
            line[index++] = '\0';
        }

        if (line[index] == '\0') {
            break;
        }

        if (parsed->argc == ARG_MAX) {
            return parsed->argc;
        }

        parsed->argv[parsed->argc++] = &line[index];
        while (line[index] != '\0' && !is_space(line[index])) {
            index++;
        }
    }

    if (parsed->argc > 0) {
        uppercase_text(parsed->argv[0]);
    }

    return parsed->argc;
}

static const struct command *find_command(const char *name)
{
    for (uint64_t i = 0; i < command_count; i++) {
        if (string_equals(name, commands[i].name)) {
            return &commands[i];
        }
    }

    return 0;
}

static struct alias_entry *find_alias(const char *name)
{
    for (uint64_t i = 0; i < ALIAS_MAX; i++) {
        if (alias_table[i].used && string_equals(alias_table[i].name, name)) {
            return &alias_table[i];
        }
    }

    return 0;
}

static void apply_alias(struct parsed_command *parsed)
{
    if (parsed->argc == 0) {
        return;
    }

    struct alias_entry *alias = find_alias(parsed->argv[0]);
    if (alias == 0) {
        return;
    }

    copy_text(parse_buffer, TERM_CAP, alias->value);
    for (uint64_t i = 1; i < parsed->argc; i++) {
        append_char(parse_buffer, TERM_CAP, ' ');
        append_text(parse_buffer, TERM_CAP, parsed->argv[i]);
    }
    (void)tokenize(parse_buffer, parsed);
}

static void pack_args(const struct parsed_command *parsed, char *dest, uint64_t dest_size)
{
    dest[0] = '\0';
    for (uint64_t i = 1; i < parsed->argc; i++) {
        if (i > 1) {
            append_char(dest, dest_size, ' ');
        }
        append_text(dest, dest_size, parsed->argv[i]);
    }
}

static void complete_command(void)
{
    if (input_len == 0 || cursor_pos != input_len) {
        return;
    }

    char upper[TERM_CAP];
    uint64_t matches = 0;
    const char *match = 0;
    copy_text(upper, sizeof(upper), input);
    uppercase_text(upper);

    for (uint64_t i = 0; i < command_count; i++) {
        if (string_starts_with(commands[i].name, upper)) {
            matches++;
            match = commands[i].name;
        }
    }

    if (matches == 1 && match != 0) {
        copy_text(input, TERM_CAP, match);
        input_len = string_length(input);
        cursor_pos = input_len;
        if (input_len + 1 < TERM_CAP) {
            input[input_len++] = ' ';
            input[input_len] = '\0';
            cursor_pos = input_len;
        }
    } else if (matches > 1) {
        push_line("MULTIPLE MATCHES");
    } else {
        push_line("NO MATCH");
    }
}

static void insert_input_char(char value)
{
    if (input_len + 1 >= TERM_CAP) {
        return;
    }

    for (uint64_t i = input_len; i > cursor_pos; i--) {
        input[i] = input[i - 1];
    }

    input[cursor_pos] = value;
    input_len++;
    cursor_pos++;
    input[input_len] = '\0';
}

static void delete_before_cursor(void)
{
    if (cursor_pos == 0 || input_len == 0) {
        return;
    }

    for (uint64_t i = cursor_pos - 1; i < input_len; i++) {
        input[i] = input[i + 1];
    }

    input_len--;
    cursor_pos--;
}

static void delete_at_cursor(void)
{
    if (cursor_pos >= input_len) {
        return;
    }

    for (uint64_t i = cursor_pos; i < input_len; i++) {
        input[i] = input[i + 1];
    }

    input_len--;
}

static void set_env_value(const char *key, const char *value)
{
    struct env_entry *slot = 0;

    for (uint64_t i = 0; i < ENV_MAX; i++) {
        if (env_table[i].used && string_equals(env_table[i].key, key)) {
            slot = &env_table[i];
        }
    }

    if (slot == 0) {
        for (uint64_t i = 0; i < ENV_MAX; i++) {
            if (!env_table[i].used) {
                slot = &env_table[i];
                slot->used = true;
                copy_text(slot->key, sizeof(slot->key), key);
                break;
            }
        }
    }

    if (slot == 0) {
        push_line("ENV TABLE FULL");
        return;
    }

    copy_text(slot->value, sizeof(slot->value), value);
}

static bool split_assignment(char *value, char **right)
{
    for (uint64_t i = 0; value[i] != '\0'; i++) {
        if (value[i] == '=') {
            value[i] = '\0';
            *right = &value[i + 1];
            return value[0] != '\0';
        }
    }

    return false;
}

static void send_bridge_command(const struct parsed_command *parsed)
{
    bridge_buffer[0] = '\0';
    append_text(bridge_buffer, TERM_CAP, "BRG ");
    append_text(bridge_buffer, TERM_CAP, parsed->argv[0]);

    for (uint64_t i = 1; i < parsed->argc; i++) {
        char lowered[VALUE_CAP];
        copy_text(lowered, sizeof(lowered), parsed->argv[i]);
        if (string_starts_with(parsed->argv[0], "PKG_")) {
            lowercase_text(lowered);
        }
        append_char(bridge_buffer, TERM_CAP, ' ');
        append_text(bridge_buffer, TERM_CAP, lowered);
    }

    (void)z_ui_send(Z_UI_MSG_COMMAND,
                    0,
                    Z_UI_CMD_BRIDGE_DATA,
                    0,
                    string_length(bridge_buffer),
                    bridge_buffer);
}

static void execute_external(const struct parsed_command *parsed)
{
    char path[VALUE_CAP];
    char arg[TERM_CAP];

    path[0] = '\0';
    append_text(path, sizeof(path), "/bin/");
    append_text(path, sizeof(path), parsed->argv[0]);
    lowercase_text(path);
    pack_args(parsed, arg, sizeof(arg));

    uint64_t pid = z_spawn(path, arg);
    if (pid == UINT64_MAX) {
        push_line("UNKNOWN COMMAND");
        return;
    }

    (void)z_wait(pid);
}

static int64_t find_char(const char *line, char needle)
{
    for (uint64_t i = 0; line[i] != '\0'; i++) {
        if (line[i] == needle) {
            return (int64_t)i;
        }
    }

    return -1;
}

static int64_t find_output_operator(const char *line, bool *append)
{
    for (uint64_t i = 0; line[i] != '\0'; i++) {
        if (line[i] == '>') {
            *append = line[i + 1] == '>';
            return (int64_t)i;
        }
    }

    return -1;
}

static void trim_copy_segment(char *dest,
                              uint64_t dest_size,
                              const char *src,
                              uint64_t start,
                              uint64_t end)
{
    uint64_t index = 0;

    if (dest_size == 0) {
        return;
    }

    while (start < end && is_space(src[start])) {
        start++;
    }

    while (end > start && is_space(src[end - 1])) {
        end--;
    }

    while (index + 1 < dest_size && start < end) {
        dest[index++] = src[start++];
    }

    dest[index] = '\0';
}

static bool read_text_file(const char *path, char *dest, uint64_t dest_size)
{
    if (dest_size == 0) {
        return false;
    }

    dest[0] = '\0';
    uint64_t fd = z_open(path);
    if (fd == UINT64_MAX) {
        return false;
    }

    uint64_t count = z_read(fd, dest, dest_size - 1);
    (void)z_close(fd);
    if (count == UINT64_MAX) {
        return false;
    }

    dest[count] = '\0';
    return true;
}

static bool write_text_file(const char *path, const char *text, bool append)
{
    char existing[IO_CAP];
    uint64_t fd;
    uint64_t written;

    existing[0] = '\0';
    if (append) {
        (void)read_text_file(path, existing, sizeof(existing));
    }

    fd = z_open(path);
    if (fd == UINT64_MAX) {
        return false;
    }

    if (append && existing[0] != '\0') {
        written = z_write(fd, existing, string_length(existing));
        if (written == UINT64_MAX) {
            (void)z_close(fd);
            return false;
        }
    }

    written = z_write(fd, text, string_length(text));
    (void)z_close(fd);
    return written != UINT64_MAX;
}

static bool command_output_ls(const struct parsed_command *parsed, char *dest, uint64_t dest_size)
{
    const char *path = parsed->argc == 1 ? cwd : parsed->argv[1];

    if (string_equals(path, "/") || string_equals(path, "")) {
        copy_text(dest, dest_size, "bin dev etc home usr");
        return true;
    }

    if (string_equals(path, "/bin") || string_equals(path, "bin")) {
        copy_text(dest, dest_size, "init session terminal settings launcher sh help sysinfo echo");
        return true;
    }

    if (string_equals(path, "/dev") || string_equals(path, "dev")) {
        copy_text(dest, dest_size, "stdin stdout stderr console");
        return true;
    }

    return false;
}

static bool evaluate_simple_output(const char *segment, char *dest, uint64_t dest_size)
{
    struct parsed_command parsed;
    char local[TERM_CAP];

    if (dest_size == 0) {
        return false;
    }
    dest[0] = '\0';

    copy_text(local, sizeof(local), segment);
    if (tokenize(local, &parsed) == 0) {
        return false;
    }

    if (string_equals(parsed.argv[0], "ECHO")) {
        for (uint64_t i = 1; i < parsed.argc; i++) {
            if (i > 1) {
                append_char(dest, dest_size, ' ');
            }
            append_text(dest, dest_size, parsed.argv[i]);
        }
        return true;
    }

    if (string_equals(parsed.argv[0], "PWD")) {
        copy_text(dest, dest_size, cwd);
        return true;
    }

    if (string_equals(parsed.argv[0], "WHOAMI")) {
        copy_text(dest, dest_size, "zenith");
        return true;
    }

    if (string_equals(parsed.argv[0], "SYSINFO")) {
        copy_text(dest, dest_size, "ZENITHOS IPC TERMINAL ONLINE");
        return true;
    }

    if (string_equals(parsed.argv[0], "LS")) {
        return command_output_ls(&parsed, dest, dest_size);
    }

    if (string_equals(parsed.argv[0], "CAT") && parsed.argc == 2) {
        return read_text_file(parsed.argv[1], dest, dest_size);
    }

    return false;
}

static bool add_output_newline(char *text, uint64_t text_size)
{
    uint64_t length = string_length(text);
    if (length + 1 >= text_size) {
        return false;
    }

    text[length] = '\n';
    text[length + 1] = '\0';
    return true;
}

static bool execute_output_redirection(const char *line, uint64_t op_index, bool append)
{
    char command_text[TERM_CAP];
    char path[TERM_CAP];
    char output[IO_CAP];
    uint64_t path_start = op_index + (append ? 2 : 1);

    trim_copy_segment(command_text, sizeof(command_text), line, 0, op_index);
    trim_copy_segment(path, sizeof(path), line, path_start, string_length(line));

    if (path[0] == '\0') {
        push_line("REDIRECT PATH REQUIRED");
        return true;
    }

    if (!evaluate_simple_output(command_text, output, sizeof(output))) {
        push_line("REDIRECT BUILTINS ONLY");
        return true;
    }

    (void)add_output_newline(output, sizeof(output));
    if (!write_text_file(path, output, append)) {
        push_line("REDIRECT WRITE FAILED");
        return true;
    }

    push_pair("WROTE ", path);
    return true;
}

static bool execute_input_redirection(const char *line, uint64_t op_index)
{
    char command_text[TERM_CAP];
    char path[TERM_CAP];
    char file_text[IO_CAP];
    struct parsed_command parsed;

    trim_copy_segment(command_text, sizeof(command_text), line, 0, op_index);
    trim_copy_segment(path, sizeof(path), line, op_index + 1, string_length(line));

    if (path[0] == '\0') {
        push_line("INPUT PATH REQUIRED");
        return true;
    }

    copy_text(parse_buffer, sizeof(parse_buffer), command_text);
    if (tokenize(parse_buffer, &parsed) == 0) {
        push_line("INPUT COMMAND REQUIRED");
        return true;
    }

    if (!string_equals(parsed.argv[0], "CAT") || parsed.argc != 1) {
        push_line("INPUT REDIRECT CAT ONLY");
        return true;
    }

    if (!read_text_file(path, file_text, sizeof(file_text))) {
        push_line("INPUT READ FAILED");
        return true;
    }

    push_line(file_text);
    return true;
}

static bool execute_pipeline(const char *line, uint64_t op_index)
{
    char left[TERM_CAP];
    char right[TERM_CAP];
    char output[IO_CAP];
    char piped[IO_CAP];
    struct parsed_command parsed;
    uint64_t fds[2];

    trim_copy_segment(left, sizeof(left), line, 0, op_index);
    trim_copy_segment(right, sizeof(right), line, op_index + 1, string_length(line));

    if (left[0] == '\0' || right[0] == '\0') {
        push_line("PIPE COMMAND REQUIRED");
        return true;
    }

    if (!evaluate_simple_output(left, output, sizeof(output))) {
        push_line("PIPE LEFT BUILTINS ONLY");
        return true;
    }

    if (z_pipe(fds) == UINT64_MAX) {
        push_line("PIPE CREATE FAILED");
        return true;
    }

    (void)z_write(fds[1], output, string_length(output));
    (void)z_close(fds[1]);

    uint64_t count = z_read(fds[0], piped, sizeof(piped) - 1);
    (void)z_close(fds[0]);
    if (count == UINT64_MAX) {
        push_line("PIPE READ FAILED");
        return true;
    }
    piped[count] = '\0';

    copy_text(parse_buffer, sizeof(parse_buffer), right);
    if (tokenize(parse_buffer, &parsed) == 0) {
        push_line("PIPE TARGET REQUIRED");
        return true;
    }

    if (string_equals(parsed.argv[0], "CAT") && parsed.argc == 1) {
        push_line(piped);
    } else if (string_equals(parsed.argv[0], "ECHO")) {
        push_line(piped);
    } else {
        push_line("PIPE TARGET BUILTINS ONLY");
    }

    return true;
}

static bool execute_shell_operator_line(const char *line)
{
    bool append = false;
    int64_t pipe_index = find_char(line, '|');
    int64_t output_index;
    int64_t input_index;

    if (pipe_index >= 0) {
        return execute_pipeline(line, (uint64_t)pipe_index);
    }

    output_index = find_output_operator(line, &append);
    if (output_index >= 0) {
        return execute_output_redirection(line, (uint64_t)output_index, append);
    }

    input_index = find_char(line, '<');
    if (input_index >= 0) {
        return execute_input_redirection(line, (uint64_t)input_index);
    }

    return false;
}

static void execute_line(const char *line)
{
    struct parsed_command parsed;

    if (execute_shell_operator_line(line)) {
        return;
    }

    copy_text(parse_buffer, sizeof(parse_buffer), line);
    if (tokenize(parse_buffer, &parsed) == 0) {
        return;
    }

    apply_alias(&parsed);

    const struct command *command = find_command(parsed.argv[0]);
    if (command == 0) {
        execute_external(&parsed);
        return;
    }

    uint64_t arg_count = parsed.argc - 1;
    if (arg_count < command->min_args || arg_count > command->max_args) {
        push_pair("USAGE ", command->help);
        return;
    }

    command->handler(command, &parsed);
}

static void handle_help(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;

    if (parsed->argc == 2) {
        const struct command *target = find_command(parsed->argv[1]);
        if (target == 0) {
            push_line("NO SUCH COMMAND");
            return;
        }
        push_line(target->help);
        return;
    }

    push_line("HELP SYSINFO ECHO CLEAR DESKTOP");
    push_line("ENV EXPORT ALIAS HISTORY WHOAMI");
    push_line("PWD CD LS CAT EXIT SETTINGS");
    push_line("PKG_INSTALL PKG_SEARCH PKG_INFO");
}

static void handle_sysinfo(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    push_line("ZENITHOS IPC TERMINAL ONLINE");
    push_line("TOKENIZER TABLE HISTORY ACTIVE");
    push_line("BRIDGE COMMANDS USE UI IPC");
}

static void handle_echo(const struct command *command, const struct parsed_command *parsed)
{
    char text[TERM_CAP];
    (void)command;
    text[0] = '\0';
    for (uint64_t i = 1; i < parsed->argc; i++) {
        if (i > 1) {
            append_char(text, sizeof(text), ' ');
        }
        append_text(text, sizeof(text), parsed->argv[i]);
    }
    push_line(text);
}

static void handle_clear(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    clear_terminal();
}

static void handle_desktop(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    push_line("SESSION OWNS TOP BAR WINDOWS");
    push_line("APPS DRAW THROUGH UI IPC");
}

static void handle_reboot(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    push_line("REBOOTING");
    redraw();
    (void)z_reboot();
    push_line("REBOOT FAILED");
}

static void handle_env(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    bool any = false;

    for (uint64_t i = 0; i < ENV_MAX; i++) {
        if (env_table[i].used) {
            any = true;
            push_pair(env_table[i].key, env_table[i].value);
        }
    }

    if (!any) {
        push_line("ENV EMPTY");
    }
}

static void handle_export(const struct command *command, const struct parsed_command *parsed)
{
    char item[TERM_CAP];
    char *right = 0;
    (void)command;

    copy_text(item, sizeof(item), parsed->argv[1]);
    if (!split_assignment(item, &right)) {
        push_line("USAGE EXPORT KEY=VALUE");
        return;
    }

    uppercase_text(item);
    set_env_value(item, right);
}

static void handle_alias(const struct command *command, const struct parsed_command *parsed)
{
    char item[TERM_CAP];
    char *right = 0;
    (void)command;

    if (parsed->argc == 1) {
        bool any = false;
        for (uint64_t i = 0; i < ALIAS_MAX; i++) {
            if (alias_table[i].used) {
                any = true;
                push_pair(alias_table[i].name, alias_table[i].value);
            }
        }
        if (!any) {
            push_line("ALIAS EMPTY");
        }
        return;
    }

    copy_text(item, sizeof(item), parsed->argv[1]);
    if (!split_assignment(item, &right)) {
        push_line("USAGE ALIAS NAME=VALUE");
        return;
    }
    uppercase_text(item);
    uppercase_text(right);

    for (uint64_t i = 0; i < ALIAS_MAX; i++) {
        if (!alias_table[i].used || string_equals(alias_table[i].name, item)) {
            alias_table[i].used = true;
            copy_text(alias_table[i].name, sizeof(alias_table[i].name), item);
            copy_text(alias_table[i].value, sizeof(alias_table[i].value), right);
            return;
        }
    }

    push_line("ALIAS TABLE FULL");
}

static void handle_history(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;

    if (history_count == 0) {
        push_line("HISTORY EMPTY");
        return;
    }

    for (uint64_t i = 0; i < history_count && i < 5; i++) {
        push_line(history[history_slot(i)]);
    }
}

static void handle_whoami(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    push_line("zenith");
}

static void handle_pwd(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    (void)z_getcwd(cwd, sizeof(cwd));
    push_line(cwd);
}

static void handle_cd(const struct command *command, const struct parsed_command *parsed)
{
    char target[VALUE_CAP];
    (void)command;
    if (parsed->argc == 1) {
        copy_text(target, sizeof(target), "/");
        (void)z_chdir(target);
        (void)z_getcwd(cwd, sizeof(cwd));
        return;
    }

    if (parsed->argv[1][0] == '/') {
        copy_text(target, sizeof(target), parsed->argv[1]);
    } else {
        copy_text(target, sizeof(target), "/");
        append_text(target, sizeof(target), parsed->argv[1]);
    }

    if (z_chdir(target) == UINT64_MAX) {
        push_line("CD FAILED");
        return;
    }
    (void)z_getcwd(cwd, sizeof(cwd));
}

static void handle_ls(const struct command *command, const struct parsed_command *parsed)
{
    const char *path = parsed->argc == 1 ? cwd : parsed->argv[1];
    (void)command;

    if (string_equals(path, "/") || string_equals(path, "")) {
        push_line("bin dev etc home usr");
    } else if (string_equals(path, "/bin") || string_equals(path, "bin")) {
        push_line("init session terminal settings");
        push_line("launcher sh help sysinfo echo");
    } else if (string_equals(path, "/dev") || string_equals(path, "dev")) {
        push_line("stdin stdout stderr console");
    } else {
        push_line("LS PATH NOT FOUND");
    }
}

static void handle_cat(const struct command *command, const struct parsed_command *parsed)
{
    char buffer[TERM_CAP];
    (void)command;

    uint64_t fd = z_open(parsed->argv[1]);
    if (fd == UINT64_MAX) {
        push_line("CAT OPEN FAILED");
        return;
    }

    uint64_t count = z_read(fd, buffer, sizeof(buffer) - 1);
    (void)z_close(fd);
    if (count == UINT64_MAX) {
        push_line("CAT READ FAILED");
        return;
    }

    buffer[count] = '\0';
    push_line(buffer);
}

static void handle_exit(const struct command *command, const struct parsed_command *parsed)
{
    (void)command;
    (void)parsed;
    z_exit(0);
}

static void handle_bridge(const struct command *command, const struct parsed_command *parsed)
{
    if (command->classification != CLASS_BRIDGE_ONLY) {
        push_line("BRIDGE CLASS ERROR");
        return;
    }

    send_bridge_command(parsed);
    push_pair("BRIDGE ", parsed->argv[0]);
}

static void commit_input(void)
{
    char line[TERM_CAP];
    copy_text(line, sizeof(line), input);
    history_add(line);
    persist_history_line(line);
    execute_line(line);
    reset_input();
}

static void handle_bridge_message(const char *text)
{
    if (string_starts_with(text, "HIST ")) {
        history_add(text + 5);
    }
}

static void handle_escape(char value)
{
    if (esc_state == 1) {
        esc_state = value == '[' ? 2 : 0;
        return;
    }

    if (esc_state != 2) {
        esc_state = 0;
        return;
    }

    if (value == 'D' && cursor_pos > 0) {
        cursor_pos--;
    } else if (value == 'C' && cursor_pos < input_len) {
        cursor_pos++;
    } else if (value == 'A') {
        history_previous();
    } else if (value == 'B') {
        history_next_line();
    } else if (value == '3') {
        esc_state = 3;
        return;
    }

    esc_state = 0;
}

static void handle_key(uint64_t key)
{
    char value = (char)(uint8_t)key;

    if (esc_state != 0) {
        if (esc_state == 3) {
            if (value == '~') {
                delete_at_cursor();
            }
            esc_state = 0;
        } else {
            handle_escape(value);
        }
        redraw();
        return;
    }

    if (value == 27) {
        esc_state = 1;
        return;
    }

    if (value == '\n' || value == '\r') {
        commit_input();
        redraw();
        return;
    }

    if (value == '\t') {
        complete_command();
        redraw();
        return;
    }

    if (value == '\b' || value == 127) {
        delete_before_cursor();
        redraw();
        return;
    }

    if (value < ' ' || value > '~') {
        return;
    }

    insert_input_char(value);
    redraw();
}

static void init_terminal_state(void)
{
    clear_terminal();
    copy_text(cwd, sizeof(cwd), "/");
    (void)z_chdir(cwd);
    (void)z_getcwd(cwd, sizeof(cwd));
    set_env_value("USER", "zenith");
    set_env_value("SHELL", "zenith-terminal");
}

int main(const char *arg)
{
    (void)arg;

    init_terminal_state();
    push_line("TERMINAL APP READY");
    push_line("TYPE HELP THEN ENTER");

    (void)z_ui_send(Z_UI_MSG_REGISTER, 0, Z_UI_ROLE_TERMINAL, 760, 472, "TERMINAL");
    request_history_load();
    redraw();

    for (;;) {
        struct z_ui_message message;
        if (z_ui_recv(&message) == 0) {
            if (message.type == Z_UI_MSG_INPUT_KEY) {
                handle_key(message.a);
            } else if (message.type == Z_UI_MSG_COMMAND && message.a == Z_UI_CMD_BRIDGE_DATA) {
                handle_bridge_message(message.text);
            } else if (message.type == Z_UI_MSG_INPUT_MOUSE) {
                (void)z_ui_send(Z_UI_MSG_NOTIFY, 0, Z_UI_CMD_NOTIFY, 0, 0, "TERMINAL FOCUSED");
            } else if (message.type == Z_UI_MSG_CLOSE) {
                z_exit(0);
            }
        } else {
            z_sleep_ticks(1);
        }
    }
}
