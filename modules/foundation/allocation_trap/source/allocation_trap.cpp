/**************************************************************************/
/* allocation_trap.cpp                                                    */
/**************************************************************************/
/*                          This file is part of:                         */
/*                                SushiDSP                                */
/*                https://github.com/SushiSystems/SushiDSP                */
/*                         https://sushisystems.io                        */
/**************************************************************************/
/* Copyright (c) 2026-present Mustafa Garip & Sushi Systems               */
/*                                                                        */
/* Licensed under the PolyForm Noncommercial License 1.0.0 (the           */
/* "License"); you may not use this file except in compliance with the    */
/* License. You may obtain a copy of the License at                       */
/*                                                                        */
/*     https://polyformproject.org/licenses/noncommercial/1.0.0           */
/*                                                                        */
/* Noncommercial use is free. Commercial use requires a separate licence  */
/* from Sushi Systems; see COMMERCIAL.md. The software is provided        */
/* "as is", without warranty of any kind.                                 */
/**************************************************************************/

/**
 * @file allocation_trap.cpp
 * @brief Defines the allocation trap and the global allocation functions it counts through.
 * @author Mustafa Garip
 */

#include <SushiDSP/foundation/allocation_trap/allocation_trap.hpp>

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
    // Each thread carries its own trap, so a case only ever counts the thread that opened its window.
    thread_local std::atomic<bool> g_trap_allocations{false};
    thread_local std::atomic<int> g_trapped_allocation_count{0};
} // namespace

namespace SushiDSP::test
{
    AllocationTrap::AllocationTrap() noexcept
    {
        g_trapped_allocation_count.store(0, std::memory_order_relaxed);
    }

    AllocationTrap::~AllocationTrap()
    {
        disarm();
    }

    void AllocationTrap::arm() noexcept
    {
        g_trap_allocations.store(true, std::memory_order_relaxed);
    }

    void AllocationTrap::disarm() noexcept
    {
        g_trap_allocations.store(false, std::memory_order_relaxed);
    }

    int AllocationTrap::count() const noexcept
    {
        return g_trapped_allocation_count.load(std::memory_order_relaxed);
    }
} // namespace SushiDSP::test

/**
 * @brief Allocates through malloc, counting the allocation when the calling thread's trap is armed.
 * @param size Bytes requested; zero is served as one byte.
 */
void* operator new(std::size_t size)
{
    if (g_trap_allocations.load(std::memory_order_relaxed))
    {
        g_trapped_allocation_count.fetch_add(1, std::memory_order_relaxed);
    }
    void* ptr = std::malloc(size != 0 ? size : 1);
    if (ptr == nullptr)
    {
        throw std::bad_alloc();
    }
    return ptr;
}

/// @brief Allocates an array through the counting operator new.
void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

/// @brief Frees memory the counting operator new allocated.
void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}

/// @brief Frees an array the counting operator new allocated.
void operator delete[](void* ptr) noexcept
{
    std::free(ptr);
}

/// @brief Frees memory the counting operator new allocated, ignoring the size.
void operator delete(void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}

/// @brief Frees an array the counting operator new allocated, ignoring the size.
void operator delete[](void* ptr, std::size_t) noexcept
{
    std::free(ptr);
}
