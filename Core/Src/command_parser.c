#include "command_parser.h"

bool command_parser_feed_byte(
    CommandParser *parser,
    uint8_t byte,
    const char **command
)
{
    if ((parser == NULL) || (command == NULL))
    {
        return false;
    }

    *command = NULL;

    if ((byte == '\r') || (byte == '\n'))
    {
        if (parser->discarding)
        {
            parser->discarding = false;
            parser->length = 0U;
            return false;
        }

        if (parser->length == 0U)
        {
            return false;
        }

        parser->buffer[parser->length] = '\0';
        *command = parser->buffer;
        parser->length = 0U;

        return true;
    }

    if (parser->discarding)
    {
        return false;
    }

    if (parser->length >=
        (COMMAND_PARSER_BUFFER_SIZE - 1U))
    {
        parser->overflow_count++;
        parser->length = 0U;
        parser->discarding = true;

        return false;
    }

    parser->buffer[parser->length] = (char)byte;
    parser->length++;

    return false;
}

void command_parser_discard_until_eol(
    CommandParser *parser
)
{
    if (parser == NULL)
    {
        return;
    }

    parser->length = 0U;
    parser->discarding = true;
}

uint32_t command_parser_get_overflow_count(
    const CommandParser *parser
)
{
    if (parser == NULL)
    {
        return 0U;
    }

    return parser->overflow_count;
}

void command_parser_init(CommandParser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    command_parser_reset(parser);
    parser->overflow_count = 0U;
}

void command_parser_reset(CommandParser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    parser->buffer[0] = '\0';
    parser->length = 0U;
    parser->discarding = false;
}
