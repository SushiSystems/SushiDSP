/**************************************************************************/
/* delegate.hpp                                                           */
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
 * @file delegate.hpp
 * @brief Declares a copyable callable held in a fixed buffer, with no heap allocation.
 * @author Mustafa Garip
 */

#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace SushiDSP
{
    /// @brief Declares the delegate; only the function-type specialisation below is defined.
    template <typename Signature, std::size_t Capacity>
    class Delegate;

    /// @brief Holds any copyable callable of up to Capacity bytes, or nothing.
    /// @tparam Capacity The bytes the callable may take; a larger one does not compile.
    template <typename Result, typename... Arguments, std::size_t Capacity>
    class Delegate<Result(Arguments...), Capacity> final
    {
        public:
            /// @brief Makes an empty delegate.
            Delegate() noexcept = default;

            /// @brief Makes an empty delegate.
            Delegate(std::nullptr_t) noexcept
            {
            }

            /// @brief Copies the callable into the buffer.
            template <typename Callable, typename = std::enable_if_t<
                                             !std::is_same<std::decay_t<Callable>, Delegate>::value>>
            Delegate(Callable&& callable)
            {
                using Stored = std::decay_t<Callable>;
                static_assert(sizeof(Stored) <= Capacity, "the callable does not fit the delegate");
                static_assert(alignof(Stored) <= alignof(std::max_align_t),
                              "the callable is over-aligned for the delegate");
                new (storage_) Stored(std::forward<Callable>(callable));
                invoke_ = [](void* storage, Arguments... arguments) -> Result
                {
                    return (*static_cast<Stored*>(storage))(std::forward<Arguments>(arguments)...);
                };
                manage_ = [](void* target, const void* source) noexcept
                {
                    if (source != nullptr)
                        new (target) Stored(*static_cast<const Stored*>(source));
                    else
                        static_cast<Stored*>(target)->~Stored();
                };
            }

            /// @brief Copies another delegate's callable.
            Delegate(const Delegate& other) noexcept
            {
                copy_from(other);
            }

            /// @brief Replaces the callable with a copy of another delegate's.
            Delegate& operator=(const Delegate& other) noexcept
            {
                if (this != &other)
                {
                    reset();
                    copy_from(other);
                }
                return *this;
            }

            /// @brief Copies another delegate's callable; the other keeps its own.
            Delegate(Delegate&& other) noexcept
            {
                copy_from(other);
            }

            /// @brief Replaces the callable with a copy of another delegate's.
            Delegate& operator=(Delegate&& other) noexcept
            {
                return *this = static_cast<const Delegate&>(other);
            }

            /// @brief Destroys the callable.
            ~Delegate()
            {
                reset();
            }

            /// @brief Returns whether the delegate holds a callable.
            explicit operator bool() const noexcept
            {
                return invoke_ != nullptr;
            }

            /// @brief Calls the callable.
            /// @pre The delegate holds a callable.
            Result operator()(Arguments... arguments) const
            {
                return invoke_(storage_, std::forward<Arguments>(arguments)...);
            }

        private:
            /// @brief Copies other's callable into this empty delegate.
            void copy_from(const Delegate& other) noexcept
            {
                if (other.invoke_ == nullptr)
                    return;
                other.manage_(storage_, other.storage_);
                invoke_ = other.invoke_;
                manage_ = other.manage_;
            }

            /// @brief Destroys the callable and leaves the delegate empty.
            void reset() noexcept
            {
                if (invoke_ == nullptr)
                    return;
                manage_(storage_, nullptr);
                invoke_ = nullptr;
                manage_ = nullptr;
            }

            /// @brief Holds the callable.
            alignas(std::max_align_t) mutable unsigned char storage_[Capacity];

            /// @brief Calls the stored callable; null when the delegate is empty.
            Result (*invoke_)(void*, Arguments...) = nullptr;

            /// @brief Copies the callable from a source, or destroys it when the source is null.
            void (*manage_)(void*, const void*) noexcept = nullptr;
    };
} // namespace SushiDSP
