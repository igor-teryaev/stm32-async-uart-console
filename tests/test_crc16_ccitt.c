#include "crc16_ccitt.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_empty_input(void)
{
    assert(
        crc16_ccitt_false_calculate(NULL, 0U) ==
        CRC16_CCITT_FALSE_INITIAL
    );
}

static void test_standard_check_value(void)
{
    static const uint8_t message[] = "123456789";

    assert(
        crc16_ccitt_false_calculate(
            message,
            sizeof(message) - 1U
        ) == 0x29B1U
    );
}

static void test_incremental_update(void)
{
    static const uint8_t first[] = "1234";
    static const uint8_t second[] = "56789";

    uint16_t crc = CRC16_CCITT_FALSE_INITIAL;

    crc = crc16_ccitt_false_update(
        crc,
        first,
        sizeof(first) - 1U
    );

    crc = crc16_ccitt_false_update(
        crc,
        second,
        sizeof(second) - 1U
    );

    assert(crc == 0x29B1U);
}

static void test_byte_order_changes_crc(void)
{
    static const uint8_t first[] = {
        0x01U, 0x02U, 0x03U
    };

    static const uint8_t reordered[] = {
        0x03U, 0x02U, 0x01U
    };

    assert(
        crc16_ccitt_false_calculate(
            first,
            sizeof(first)
        ) !=
        crc16_ccitt_false_calculate(
            reordered,
            sizeof(reordered)
        )
    );
}

static void test_single_bit_error_changes_crc(void)
{
    static const uint8_t original[] = {
        0x10U, 0x20U, 0x30U, 0x40U
    };

    static const uint8_t corrupted[] = {
        0x10U, 0x20U, 0x31U, 0x40U
    };

    assert(
        crc16_ccitt_false_calculate(
            original,
            sizeof(original)
        ) !=
        crc16_ccitt_false_calculate(
            corrupted,
            sizeof(corrupted)
        )
    );
}

static void test_zero_length_preserves_state(void)
{
    uint16_t crc = 0x1234U;

    assert(
        crc16_ccitt_false_update(
            crc,
            NULL,
            0U
        ) == crc
    );
}

int main(void)
{
    test_empty_input();
    test_standard_check_value();
    test_incremental_update();
    test_byte_order_changes_crc();
    test_single_bit_error_changes_crc();
    test_zero_length_preserves_state();

    puts("All CRC-16/CCITT-FALSE tests passed.");
    return 0;
}
