#include "command_parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_complete_command(void)
{
    CommandParser parser;
    const char *command = NULL;

    command_parser_init(&parser);

    assert(!command_parser_feed_byte(
        &parser, 'L', &command));
    assert(!command_parser_feed_byte(
        &parser, 'E', &command));
    assert(!command_parser_feed_byte(
        &parser, 'D', &command));

    assert(command_parser_feed_byte(
        &parser, '\n', &command));

    assert(command != NULL);
    assert(strcmp(command, "LED") == 0);
    assert(
        command_parser_get_overflow_count(&parser) == 0U
    );
}

static void test_empty_lines_are_ignored(void)
{
    CommandParser parser;
    const char *command = NULL;

    command_parser_init(&parser);

    assert(!command_parser_feed_byte(
        &parser, '\n', &command));
    assert(command == NULL);

    assert(!command_parser_feed_byte(
        &parser, '\r', &command));
    assert(command == NULL);
}

static void test_literal_backslash_n_is_text(void)
{
    CommandParser parser;
    const char *command = NULL;

    command_parser_init(&parser);

    assert(!command_parser_feed_byte(
        &parser, '\\', &command));
    assert(!command_parser_feed_byte(
        &parser, 'n', &command));

    /* A real newline completes the accumulated command. */
    assert(command_parser_feed_byte(
        &parser, '\n', &command));

    assert(command != NULL);
    assert(strcmp(command, "\\n") == 0);
}

int main(void)
{
    test_complete_command();
    test_empty_lines_are_ignored();
    test_literal_backslash_n_is_text();

    puts("All command parser tests passed.");
    return 0;
}
