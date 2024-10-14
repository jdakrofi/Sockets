/**
* This code defines a lock-free queue (LF Queue) using C++ templates. 
* It is a circular buffer queue designed for FIFO access to data.
*/

#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include "macros.h"

using namespace std;
/**
 * This encapsulates the LFQueue class within the Common namespace to avoid naming conflicts
 */
namespace Common{
    // Declaring a template class LFQueue that can store elements of any type T.
    // Final keyword prevents class from being inherited by other classes.
    template<typename T> class LFQueue final{
        public:
            /**
             * The constructor accepts num_elems, whic determines the size of the internal storage(store_),
             * and it pre-allocates a vector of size num_elems filled with deadu
             */
            explicit LFQueue(size_t num_elems): store_(num_elems, T()){}

            /** 
             * Returns a point to the next element in the vector where a new item will be written.
             * It references the element at the next_write_index_ whish is atomic, ensuring thread safety.
            */
            auto getNextToWrite() noexcept {
                return &store_[next_write_index_];
            }

            /**
             * Advances the next_write_index_ in a cirular fashion using modulo to wrap around and increments the num_elements_ count.
             * This allows the queue to work in a circular buffer manner.
             */
            auto updateWriteIndex() noexcept {
                next_write_index_ = (next_write_index_ + 1) % store_.size();
                num_elements_++; 
            }

            /**
             * Returns a pointer to the next element in the queue to be read
             * If the queue is empty(size == 0), it returns null ptr
             */
            auto getNextToRead() const noexcept -> const T*{
                return (size() ? &store_[next_read_index_] : nullptr);
            }

            /**
             * Advances the next_read_index_ in a circular fashion, wrapping around at the end of the vector and decrements the num_elements_ count
             * The ASSERT macro checks that there are elements in the queue (i.e. num_elements_ != 0) befire advancing the index.
             * This is a saftey check to ensure you don't read form an empty queue.
             */        
            auto updateReadIndex() noexcept {
                next_read_index_ = (next_read_index + 1) % store_.size();
                ASSERT(num_elements_ != 0, "Read and invalid element in:" + to_string(pthread_self()));
                num_elements_--;
            }

            /**
             * Returns the current number of elements in the queue by reading the value of num_elements_
             * num_elements_ is atomic and thread-safe
             */
            auto size() const noexcept{
                return num_elements_.load();
            }

            /*
             * The default constructor and copy/move constructors and assignment operators are deleted.
             * This prevents:
             * Instantiating LFQueue without arguments
             *  Copying or moving the queue (likely due to the use of std::atomic, which is non-copyable/non-movable).
             * The reason for this is that a lock-free queue is often used in multithreaded environments,
             *  and copying/moving could lead to inconsistencies or undefined behavior.
            */
            LFQueue() = delete;
            LFQueue(const LFQueue &) = delete;
            LFQueue(const LFQueue &&) = delete;
            LFQueue &operator=(const LFQueue &) = delete;
            LFQueue &operator=(const LFQueue &&) = delete;

            private:
                // Holds the queue's elements
                vector<T> store_; 

                // An atomic variable that tracks the index where the next element will be written.
                atomic<size_t> next_write_index_ = {0}; 

                // An atomic variable that tracks the index where the next element will be read from.
                atomic<size_t> next_read_index_ = {0};

                // An atomic variable that tracks the number of elements currently in the queue
                atomix<size_t> num_elements_ = {0}; 

    };

}