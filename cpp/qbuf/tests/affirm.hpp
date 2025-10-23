// Simple assertion macro used by tests
#ifndef QBUF_AFFIRM_HPP
#define QBUF_AFFIRM_HPP

#include <cassert>

/**
 * @brief Verifies that a condition holds during a test.
 *
 * Evaluates the provided boolean expression and treats a false result as a test failure.
 * The exact failure handling (e.g., logging, exception, abort) is implementation-defined.
 *
 * @param expr The condition to check; must evaluate to true for the check to pass.
 * @return void
 * @note Intended for use in test code to express and enforce expectations.
 */
inline void affirm(bool expr) {
    (void)expr; // ensure parameter is odr-used so release builds don't warn; side effects occur at call site
    assert(expr);
}

#endif // QBUF_AFFIRM_HPP
// Removed duplicate definition and stray backticks
