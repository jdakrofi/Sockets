/**
 * This code defines a lock-free logging system within the Common namespace.
 * The Logger writes various types of log messages to file in a thread safe manner.
 * It uses a background thread to handle writing to the log file asynchronously.
 * It utilises a lock-free queue (implemented by LFQueue) to avoid traditional thread synchronisation mechanisms like locks
 * This makes the logging system more efficient in multithreaded environments
 */

#pragma once
#include <string>
#include <fstream>
#include <cstdio>

#include "macros.h"
#include "lf_queue.h"
#include "threads_utils.h"
#include "time_utils.h"

namespace Common{
    //This constant sets the size of the lock-free queue for log elements to be 8 MB
    constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

    /**
     * This Enum class defines the different types of data that can be logged.
     */
    enum class LogType : int8_t{
        CHAR  = 0,
        INTEGER = 1,
        LONG_INTEGER = 2,
        LONG_LONG_INTEGER = 3,
        UNSIGNED_INTEGER = 4,
        UNSIGNED_LONG_INTEGER = 5,
        UNSIGNED_LONG_LONG_INTEGER = 6,
        FLOAT = 7,
        DOUBLE = 8
    };

    /**
     * This struct reprents a single log entry.
     * It contains a LogType to indicate the type of data being logged and a union that hold the actual data
     * The union data structure stores different data types, but only one at time
     * This helps in optimising memory usage when only one value needs to be store per log entry
     */
    struct LogElement{
        LogType type_ = LogType::CHAR;
        union{
            char c;
            int i;
            long l;
            long long ll;
            unsigned u;
            unsigned long ul;
            unsigned long long ull;
            float f;
            double d;
        } u_;
    };

    /**
     * The Logger class is responsible for writing log entries to a file, using a lock-free queue to buffer the log data.
     * It operates in a background thread to ensure that the main thread in not blocked bu the file I/O operations
     */
    class Logger final{
        public:
            /**
             * This function continously consumes log entries from the lock-free queue(queue_) and writes them to the log file(file_)
             */
            auto flushQueue() noexcept{
                while(running_){
                    for(auto next = queue_.getNextToRead(); queue_.size() && next; next = queue_.getNextToRead()){
                        //Processing each element in the queue based on its LogType and writing the corresponding data to file
                        switch(next->type_){
                            case LogType::CHAR:
                                file_ << next->u_.c;
                                break;
                            case LogType::INTEGER:
                                file_ << next->u_.i;
                                break;
                            case LogType::LONG_INTEGER:
                                file_ << next->u_.l;
                                break;
                            case LogType::LONG_LONG_INTEGER:
                                file_ << next->u_.ll;
                                break;
                            case LogType::UNSIGNED_INTEGER:
                                file_ << next->u_.u;
                                break;
                            case LogType::UNSIGNED_LONG_INTEGER:
                                file_ << next-> u_.ul;
                                break;
                            case LogType::UNSIGNED_LONG_LONG_INTEGER:
                                file_ << next->u_.ull;
                                break;
                            case LogType::FLOAT:
                                file_ << next->u_.f;
                                break;
                            case LogType::DOUBLE:
                                file_ <<  next->u_.d;
                                break;
                        }
                        queue_.updateReadIndex();
                    }
                    // After writing to file, the flush() method ensures that the data is physically written to the file
                    file_.flush();

                    // The thread sleeps for a short durations between checks to avoid excessive CPU usage
                    using namespace std::literals::chrono_literals;
                    this_thread::sleep_for(10ms);
                }
            }

            /**
             * Logger Constructor.
             * Initialises the logger, opens the the log file, and starts a background thread to write log entries from the queue
             */
            explicit Logger(const string &file_name) : file_name_(file_name), queue_(LOG_QUEUE_SIZE){
                // Log file opened using std::ofstream
                file_.open(file_name);
                // ASSERT() implemented in macro.h. 
                // Is it is used to check that the file was opened successfully
                ASSERT(file_.is_open(), "Could not open log file:" + file_name);

                // The flushQueue() function above is run in a separate thread
                // This is thread is created by calling createAndStartThread in thread_utils.h
                // This allows log messages to be written asynchronously in the background
                logger_thread_ = createAndStartThread(-1, "Common/Logger " + file_name_, [this]() {flushQueue(); });

                // Is it is used to check that the background thread was started successfully
                ASSERT(logger_thread_ != nullptr, "Failed to start Logger thread.");
            }

            /**
             * Logger Destructor
             * It ensures that the logger is properly shut down when the object is destroyed
             * Before exiting it waits for all entries in the queue to be flushed to the log file by checking the queue size
             * The backgrounf thread is joined to ensure it has completed its work before destruction
             * The log file is then closed.
             */
            ~Logger() {
                string time_str;
                cerr << Common::getCurrentTimeStr(&time_str) << " Flushing and closing Logger for " << file_name_ << endl;

                while(queue_.size()){
                    using namespace std::literals::chrono_literals;
                    this_thread::sleep_for(1s);
                }
                running_ = false;
                logger_thread_->join();

                file_.close();
                cerr << Common::getCurrentTimeStr(&time_str) << " Logger for " << file_name_ <<  " exiting." << endl;
            }

            /**
             * These overloaded methods allow different types of data to be pushed into the log queue
             * These methods create a LogElement based on the data type and push it into the lock-free queue(queue_)
             * Each method handles a specific data type
             */
            auto pushValue(const LogElement &log_element) noexcept{
                *(queue_.getNextToWrite()) = log_element;
                queue_.updateWriteIndex();
            }

            auto pushValue(const char value) noexcept{
                pushValue(LogElement{LogType::CHAR, {.c = value}});
            }

            auto pushValue(const int value) noexcept{
                pushValue(LogElement{LogType::INTEGER, {.i = value }});
            }

            auto pushValue(const long value) noexcept{
                pushValue(LogElement{LogType::LONG_INTEGER, {.l = value}});
            }

            auto pushValue(const long long value) noexcept{
                pushValue(LogElement{LogType::LONG_LONG_INTEGER, {.ll = value}});
            }

            auto pushValue(const unsigned value) noexcept{
                pushValue(LogElement{LogType::UNSIGNED_INTEGER, {.u = value}});
            }

            auto pushValue(const unsigned long value) noexcept{
                pushValue(LogElement{LogType::UNSIGNED_LONG_INTEGER, {.ul = value}});
            }

            auto pushValue(const unsigned long long value) noexcept{
                pushValue(LogElement{LogType::UNSIGNED_LONG_LONG_INTEGER, {.ull = value}});
            }

            auto pushValue(const float value) noexcept{
                pushValue(LogElement{LogType::FLOAT, {.f = value}});
            }

            auto pushValue(const double value) noexcept{
                pushValue(LogElement{LogType::DOUBLE, {.d = value}});
            }

            /**
             * This method is designed to log null terminated C-style strings represented by const char *value. 
             * (*value) is essential an array of characters ending with a null character '\0'
             * The function breaks down the string into individual characters and logs them separately.
             */
            auto pushValue(const char *value) noexcept{
                // while loop iterates over each character of the string until the null character is reached
                // '\0' = false
                while (*value){
                    // pushValue(char) is called to log a single character
                    pushValue(*value);
                    //advance the pointer to the next character
                    ++value;
                }
            }

            /**
             * This function accepts a reference to a std::string (&value).
             * It allows a std::string to be passed to the logging system by converting it to a C-style string(const char *).
             * value.c_str() performs the convertion
             * This C-style string is then logged character by character by calling pushValue(const char *)
             */
            auto pushValue(const string &value) noexcept{
                pushValue(value.c_str());
            }

            /**
             * This logging function works with a format string (const char *s) and variadic list of arguments (T value, A...args)
             * Each symbol '%' in the format string(s) is replaced be a value from the provide arguments
             */

            // The template function allows logging with an arbitrary number of arguments (T value, A...args)
            template<typename T, typename...A>
            auto log(const char *s, const T &value, A...args) noexcept{
                // iterarate over the string s
                while(*s){

                    // when the '%' if the next character is another '%' it is treated as a literal '%' (escape sequence) 
                    // s is incremented to move past it
                    if(*s == '%'){
                        if(UNLIKELY(*(s + 1) == '%')){
                            ++s;
                        }
                        // If not'%%' the '%' is replaced with the current argument (value)
                        // The remaining format string and arguments are handled recursively
                        else{
                            pushValue(value);
                            log(s + 1, args...);
                            return;
                        }
                    }
                    // Regular characters (not %) are logged directly
                    // The points s is incremented to the process the next characte
                    pushValue(*s++); 
                }

                // If there are more arguments than '%' placeholders in the format string, a fatal error is raised 
                FATAL("extra arguments provided to log()");  
            }

            /**
             * This version of the log() function is called when there are no more argments left to substitute.
             * It continues to processing the format string
             * If a single '%' is found, it checks to for '%%' to handle the escape sequence.
             * Is a single '%' is found without a corresponding argument, it raises a fatal error
             * This is because there's a missing argument for the '%' placeholder.
             */
             /*
            How It Works:
            The main log function processes a format string with a variable number of arguments
            It replaces % placeholders with the provided values, handling escape sequences (%%) for literal % symbols.
            Once all arguments are processed, the overloaded log function without arguments finishes processing the 
            rest of the format string, ensuring there are no missing or extra arguments.
             */
            auto log(const char *s) noexcept{
                while(*s){
                    if(*s == '%'){
                        if(UNLIKELY(*(s + 1) == '%')){
                            ++s;
                        }else{
                            FATAL("missing arguments to log()");
                        }
                    }

                    pushValue(*s++);
                }
            }
           

            /**
             * These lines delelte the default constructor, copy constructor and assignment operators
             * This prevents accidental copying and moving of the Logger object, ensuring only one instance of the logger can exist per log file
             */
            Logger() = delete;
            Logger(const Logger &) = delete;
            Logger(const Logger &&) = delete;
            Logger &operator=(const Logger &) = delete;
            Logger &operator=(const Logger &&) = delete;


        private:
            // Stores the name of the log file
            const string file_name_;
            // A file stream used for writing lof entries to the file
            ofstream file_;

            // A lock-free queue that stores LogElement objects, allowing multiple threads to push log entries
            // While the background thread writes them to the file
            LFQueue<LogElement> queue_;

            // An atomic boolean that controls whether the logger is running
            atomic<bool> running_ = {true};

            // A pointer to the background thread responsible for writing log entries.
            thread *logger_thread_ = nullptr;

    };
}