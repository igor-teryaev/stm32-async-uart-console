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

static void test_maximum_length_command(void)
{
    CommandParser parser;
    const char *command = NULL;
    char expected[COMMAND_PARSER_BUFFER_SIZE];

    command_parser_init(&parser);

    for (size_t i = 0U;
         i < (COMMAND_PARSER_BUFFER_SIZE - 1U);
         i++)
    {
        expected[i] = 'A';

        assert(!command_parser_feed_byte(
            &parser, 'A', &command));
    }

    expected[COMMAND_PARSER_BUFFER_SIZE - 1U] = '\0';

    assert(command_parser_feed_byte(
        &parser, '\n', &command));

    assert(command != NULL);
    assert(strcmp(command, expected) == 0);
    assert(
        command_parser_get_overflow_count(&parser) == 0U
    );
}

static void test_overlength_command_is_discarded(void)
{
    CommandParser parser;
    const char *command = NULL;

    command_parser_init(&parser);

    for (size_t i = 0U;
         i < COMMAND_PARSER_BUFFER_SIZE;
         i++)
    {
        assert(!command_parser_feed_byte(
            &parser, 'A', &command));
    }

    assert(
        command_parser_get_overflow_count(&parser) == 1U
    );

    /* Additional bytes from the same damaged line are ignored. */
    for (size_t i = 0U; i < 20U; i++)
    {
        assert(!command_parser_feed_byte(
            &parser, 'B', &command));
    }

    assert(
        command_parser_get_overflow_count(&parser) == 1U
    );

    /* End discard mode. This line must not produce a command. */
    assert(!command_parser_feed_byte(
        &parser, '\n', &command));

    /* The following complete line must be accepted normally. */
    assert(!command_parser_feed_byte(
        &parser, 'O', &command));
    assert(!command_parser_feed_byte(
        &parser, 'K', &command));
    assert(command_parser_feed_byte(
        &parser, '\n', &command));

    assert(command != NULL);
    assert(strcmp(command, "OK") == 0);
}

static void test_reset_preserves_diagnostics(void)
{
    CommandParser parser;
    const char *command = NULL;

    command_parser_init(&parser);

    //Передати 32 символи 'A', щоб отримати overflow
    for (size_t i = 0U;
             i < (COMMAND_PARSER_BUFFER_SIZE);
             i++)
        {
            assert(!command_parser_feed_byte(
                &parser, 'A', &command));
        }

    //overflow_count == 1U
    assert(command_parser_get_overflow_count(&parser) == 1U);
    command_parser_reset(&parser);
    //все ще overflow_count == 1U і після ресету
    assert(command_parser_get_overflow_count(&parser) == 1U);

    //передати команду ОК
    assert(!command_parser_feed_byte(
        &parser, 'O', &command));
    assert(!command_parser_feed_byte(
        &parser, 'K', &command));
    assert(command_parser_feed_byte(
        &parser, '\n', &command));

    //Перевірити, що команда "OK" прийнята — тобто reset() очистив discarding.
    assert(command != NULL);
    assert(strcmp(command, "OK") == 0);

    //Перевірити, що після повної повторної ініціалізації overflow_count == 0U.
    command_parser_init(&parser);
    assert(command_parser_get_overflow_count(&parser) == 0U);
}

static void test_discard_until_eol_resynchronizes(void)
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

    /* Simulate loss of bytes in the RX transport. */
    command_parser_discard_until_eol(&parser);

    assert(!command_parser_feed_byte(
        &parser, ' ', &command));
    assert(!command_parser_feed_byte(
        &parser, 'O', &command));
    assert(!command_parser_feed_byte(
        &parser, 'N', &command));

    /* The damaged line must not become a command. */
    assert(!command_parser_feed_byte(
        &parser, '\n', &command));

    /* The next complete line must work normally. */
    assert(!command_parser_feed_byte(
        &parser, 'O', &command));
    assert(!command_parser_feed_byte(
        &parser, 'K', &command));
    assert(command_parser_feed_byte(
        &parser, '\n', &command));

    assert(command != NULL);
    assert(strcmp(command, "OK") == 0);
}

static void test_parser_instances_are_independent(void)
{
    CommandParser first;
    CommandParser second;

    const char *first_command = NULL;
    const char *second_command = NULL;

    command_parser_init(&first);
    command_parser_init(&second);

    assert(!command_parser_feed_byte(
        &first, 'A', &first_command));
    assert(!command_parser_feed_byte(
        &second, 'B', &second_command));

    assert(command_parser_feed_byte(
        &first, '\n', &first_command));
    assert(strcmp(first_command, "A") == 0);

    assert(command_parser_feed_byte(
        &second, '\n', &second_command));
    assert(strcmp(second_command, "B") == 0);

    assert(
        command_parser_get_overflow_count(&first) == 0U
    );
    assert(
        command_parser_get_overflow_count(&second) == 0U
    );
}

int main(void)
{
    test_complete_command();
    test_empty_lines_are_ignored();
    test_literal_backslash_n_is_text();
    test_maximum_length_command();
    test_overlength_command_is_discarded();
    test_reset_preserves_diagnostics();
    test_discard_until_eol_resynchronizes();
    test_parser_instances_are_independent();

    puts("All command parser tests passed.");
    return 0;
}
