#include "protocol_sequence_tracker.h"

#include <assert.h>
#include <stdio.h>

static void test_initial_state(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(!tracker.initialized);
    assert(tracker.accepted_count == 0U);
    assert(tracker.duplicate_count == 0U);
    assert(tracker.out_of_order_count == 0U);
}

static void test_first_sequence_is_accepted(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            100U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		100U
    ));

    assert(tracker.initialized);
    assert(tracker.last_accepted_sequence == 100U);
    assert(tracker.expected_sequence == 101U);
    assert(tracker.accepted_count == 1U);
}

static void test_expected_sequence_is_accepted(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            41U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		41U
    ));

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            42U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		42U
    ));

    assert(tracker.last_accepted_sequence == 42U);
    assert(tracker.expected_sequence == 43U);
    assert(tracker.accepted_count == 2U);
}

static void test_duplicate_is_not_accepted_again(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            42U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		42U
    ));

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            42U
        ) == PROTOCOL_SEQUENCE_RESULT_DUPLICATE
    );

    assert(tracker.accepted_count == 1U);
    assert(tracker.duplicate_count == 1U);
    assert(tracker.expected_sequence == 43U);
}

static void test_out_of_order_sequence(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            10U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		10U
    ));

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            12U
        ) ==
        PROTOCOL_SEQUENCE_RESULT_OUT_OF_ORDER
    );

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            9U
        ) ==
        PROTOCOL_SEQUENCE_RESULT_OUT_OF_ORDER
    );

    assert(tracker.accepted_count == 1U);
    assert(tracker.out_of_order_count == 2U);
    assert(tracker.expected_sequence == 11U);
}

static void test_wrap_around(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            UINT16_MAX
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		UINT16_MAX
    ));

    assert(tracker.expected_sequence == 0U);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            0U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		0U
    ));

    assert(tracker.last_accepted_sequence == 0U);
    assert(tracker.expected_sequence == 1U);
}

static void test_duplicate_at_wrap_boundary(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            UINT16_MAX
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		UINT16_MAX
    ));

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            UINT16_MAX
        ) == PROTOCOL_SEQUENCE_RESULT_DUPLICATE
    );

    assert(tracker.expected_sequence == 0U);
}

static void test_session_reset_preserves_diagnostics(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            10U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(protocol_sequence_tracker_commit(
        &tracker,
		10U
    ));

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            10U
        ) == PROTOCOL_SEQUENCE_RESULT_DUPLICATE
    );

    protocol_sequence_tracker_reset_session(
        &tracker
    );

    assert(!tracker.initialized);
    assert(tracker.accepted_count == 1U);
    assert(tracker.duplicate_count == 1U);

    /*
     * A new session may start from any sequence.
     */
    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            500U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );
    assert(protocol_sequence_tracker_commit(
        &tracker,
		500U
    ));

    assert(tracker.expected_sequence == 501U);
}

static void test_invalid_tracker(void)
{
    assert(
        protocol_sequence_tracker_classify(
            NULL,
            1U
        ) == PROTOCOL_SEQUENCE_RESULT_INVALID
    );

    assert(!protocol_sequence_tracker_commit(
        NULL,
        1U
    ));

    protocol_sequence_tracker_init(NULL);
    protocol_sequence_tracker_reset_session(NULL);
}

static void test_retry_remains_new_without_commit(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            42U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    /*
     * Application was temporarily busy:
     * no commit was performed.
     */
    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            42U
        ) == PROTOCOL_SEQUENCE_RESULT_NEW
    );

    assert(!tracker.initialized);
    assert(tracker.accepted_count == 0U);

    assert(protocol_sequence_tracker_commit(
        &tracker,
        42U
    ));

    assert(
        protocol_sequence_tracker_classify(
            &tracker,
            42U
        ) ==
        PROTOCOL_SEQUENCE_RESULT_DUPLICATE
    );
}

static void test_commit_rejects_unexpected_sequence(void)
{
    ProtocolSequenceTracker tracker;

    protocol_sequence_tracker_init(&tracker);

    assert(protocol_sequence_tracker_commit(
        &tracker,
        10U
    ));

    assert(!protocol_sequence_tracker_commit(
        &tracker,
        12U
    ));

    assert(tracker.last_accepted_sequence == 10U);
    assert(tracker.expected_sequence == 11U);
    assert(tracker.accepted_count == 1U);
}

int main(void)
{
    test_initial_state();
    test_first_sequence_is_accepted();
    test_expected_sequence_is_accepted();
    test_duplicate_is_not_accepted_again();
    test_out_of_order_sequence();
    test_wrap_around();
    test_duplicate_at_wrap_boundary();
    test_session_reset_preserves_diagnostics();
    test_invalid_tracker();
    test_commit_rejects_unexpected_sequence();
    test_retry_remains_new_without_commit();

    puts("All protocol sequence tracker tests passed.");
    return 0;
}
