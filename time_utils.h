/**
 * This code defines a set of utility functions and constants within the Common namespace.
 * These functions work with time in different units using the <chrono> library
 */
#pragma once
#include <string>
#include <chrono> // For working with time durations and clocks in a type-safe manner
#include <ctime> // Provides C-style time functions, such as converting time points into human readable strings

using namespace std;

/**
 * Common namespace is defined to encapsulate functionality
 * Helps to prevent name conflicts with other code that might use similar names
 */
namespace Common {
    /**
     * Nanos is a new type alias that represnts a 64-bit integer.
     * It will be used to store time durations measured in nanoseconds
     */ 
    typedef int64_t Nanos;

    // A series of constants used to facilitate converting between different time units
    constexpr Nanos NANOS_TO_MICROS = 1000; // 1000 nanoseconds in 1 microsecond
    constexpr Nanos MICROS_TO_MILLIS = 1000; // 1000 microseconds in 1 millisecond
    constexpr Nanos MILLIS_TO_SECS = 1000; // 1000 milliseconds in 1 second

    // calculated conversions for nanoseconds to milliseconds and seconds
    constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
    constexpr Nanos NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

    /**
     * This functions returns te current time in nanoseconds since the Unix epoch (01/01/1970)
     * The noexcept specifier indicates that this function does not throw any exceptions, which can help with performance optimizations
     */
    inline auto getCurrentNanos() noexcept{
        // chrono::duration_cast<chrono::nanoseconds> - converts time durtion
        // std::chrono::system_clock::now(): Gets the current time.
        // time_since_epoch(): Returns the time passed since the Unix epoch.
        // count(): Returns the number of nanoseconds as an integer.

        return chrono::duration_cast<chrono::nanoseconds>(chrono::system_clock::now().time_since_epoch()).count();
    }

    /**
     * This functions converts the current time to string format and modifies the string pointer (time_str)
     */
    inline auto& getCurrentTimeStr(string* time_str){
        // to_time_t(): Converts the time to std::time_t (used for C-style time functions).
        const auto time = chrono::system_clock::to_time_t(chrono::system_clock::now());

        // ctime(&time): Converts the time_t type value into a human-readable string (e.g., "Thu Sep 20 14:55:02 2023").
        // time_str->assign(ctime(&time)): Assigns the string representation to the provided std::string object
        time_str -> assign(ctime(&time));
        if(!time_str->empty())
            //Ensures the newline character at the end of the string (added by ctime()) is replaced with a null character ('\0'), thus removing it.
            time_str->at(time_str->length()-1) ='\0';

        return *time_str;
    }
}