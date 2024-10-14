/**
 * This code defines utility macros and functions that are typically used to improve performance and handle errors.
 * They are typically used in systems programming
 * They are used because performance and reliability are critical.
 * They allow the program to fail fast when conditions are violated, making it easier to debug
 */
#pragma once
#include <cstring>
#include <iostream>

using namespace std;

/**
 * These macros use the GCC/Clang built-in function __builtin_expect.
 * This enables one to provide hints to the compiler about which condition is more likely to be true.
 * This improves branch prediction in the CPU, helping the compiler to optimise the generated machine code.
 * They improve performance by guiding the compiler to anticipate the most common path
*/

// Hints that the expression x is expected to be TRUE most of the time
#define LIKELY(x) __builtin_expect(!!(x), 1)

// Hints that the expression x is expected to be FALSE most of the time
#define UNLIKELY(x) __builtin_expect(!!(x), 0)


/**
 * This macro is useful for validating conditions at runtime that must always be true.
 * If cond == false, the error message is printed the program is terminated with the error code "EXIT_FAILURE"
 * cond is wrapped in the UNLIKELY() macro, hinting to the complier that the condition is expected to be TRUE most of the time (i.e. failure unlikely)
 */
inline auto ASSERT(bool cond,const string &msg) noexcept{
    if (UNLIKELY(!cond)){
        cerr << "ASSERT : " << msg << endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * This macros is called when a critical error occurs, and the program cannot continue running.
 * This does not check any conditions
 * It is used to indicate a unrecoverabale error
 */
inline auto FATAL(const string &msg) noexcept{
    cerr << "FATAL : " << msg << endl;
    exit(EXIT_FAILURE);
}

