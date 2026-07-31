/*
 * command_parser.h
 *
 *  Created on: 31 Jul 2026
 *      Author: Kto-to
 */

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COMMAND_PARSER_BUFFER_SIZE 32U

typedef struct
{
    char buffer[COMMAND_PARSER_BUFFER_SIZE];
    size_t length;
    uint32_t overflow_count;
    bool discarding;
} CommandParser;

void command_parser_init(CommandParser *parser);

bool command_parser_feed_byte(
    CommandParser *parser,
    uint8_t byte,
    const char **command
);

void command_parser_discard_until_eol(
    CommandParser *parser
);

uint32_t command_parser_get_overflow_count(
    const CommandParser *parser
);

void command_parser_reset(
    CommandParser *parser
);

#endif
