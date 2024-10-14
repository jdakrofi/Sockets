#pragma once
#include<cstdint>
#include<vector>
#include<string>

#include "macros.h"

namespace Common{
    template<typename T>
    class MemPool final {
        public:
            explicit MemPool(size_t num_elems): store_(num_elems, {T(), true}){
                ASSERT(reinterpret_cast<const ObjectBlock *>(&(store_[0].object_)) == &(store_[0]), "T object should be first member of ObjectBlock.");
            }// explicit MemPool(size_t num_elems)

            template<typename... Args> T* allocate(Args... args) noexcept{
                auto obj_block = &(store_[next_free_index_]);
                ASSERT(obj_block->is_free_, "Expected free ObjectBlock at index:" + to_string(next_free_index_));
                T *ret = &(obj_block->object_);
                ret = new(ret) T(args...);
                obj_block->is_free_ = false;

                updateNextFreeIndex();
                return ret;
            }// template<typename... Args> T* allocate(Args... args) 

            auto deallocate(const T *elem) noexcept{
                const auto elem_index = (reinterpret_cast<const ObjectBlock *>(elem) - &store_[0]);
                ASSERT(elem_index >= 0 && static_cast<size_t>(elem_index) <store_.size(),
                 "Element being deallocated does not belong to this Memory pool.");
                ASSERT(!store_[elem_index].is_free, "Expected in-use ObjectBlock at index:" + to_string(elem_index)); 
                 store_[elem_index].is_free_ = true;
            }// auto deallocate(const T *elem

            MemPool() = delete;
            MemPool(const MemPool &) = delete;
            MemPool(const MemPool &&) = delete;
            MemPool &operator=(const MemPool &) = delete;
            MemPool &operator=(const MemePool &&) = delete;

            private:
                auto updateNextFreeIndex() noexcept{
                    const auto initial_free_index = next_free_index_;
                    while( !store_[next_free_index_].is_free_){
                        ++next_free_index;
                        if(UNLIKELY(next_free_index_ == store_.size()))
                            next_free_index_ = 0;


                        if(UNLIKELY(initial_free_index == next_free_index_))
                            ASSERT(initial_free_index != next_free_index_, "Memory Pool out of space.");

                    }//while( !store_[next_free_index_].is_free_)
                }// auto updateNextFreeIndex() noexcept

                struct ObjectBlock{
                    T object_;
                    bool is_free_ = true;

                };// struct ObjectBlock

                vector<ObjectBlock> store_;

                size_t next_free_index_ =0;

    };//class MemPool final

}// namespace Common