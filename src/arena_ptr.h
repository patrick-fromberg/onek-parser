#pragma once

#include <memory>
#include <memory_resource>

namespace onek {
    class ptr_base;
    class arena_handle;
    template<typename>
    class arena_ptr;

    class arena {// todo: extend buffer chain on buffer overflow
        static inline constexpr size_t MIN_SIZE = 10000000;
        std::pmr::monotonic_buffer_resource mbr{MIN_SIZE};
        std::pmr::polymorphic_allocator<std::byte> pa{&mbr};

        friend class arena_handle;
        friend class arena_destroyer;
        template<typename T, typename... Args>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, Args &&...);
        template<typename T>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, T const &);
    };

    class ptr_base {
        public:
        arena *arena_ = nullptr;
        ptr_base(arena *arena_) : arena_(arena_) {}

        friend arena_handle;
        template<typename T, typename... Args>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, Args &&...);
        template<typename T>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, T const &);
    };

    // exported interface

    class arena_handle {
        arena *arena_ = nullptr;

        public:
        arena_handle() = default;
        arena_handle(ptr_base const &base) : arena_(base.arena_){};

        friend class arena_destroyer;
        template<typename T, typename... Args>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, Args &&...);
        template<typename T>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, T const &);
    };

    class arena_destroyer {
        arena *arena_;

        public:
        arena_destroyer(arena_handle const &handle) : arena_(handle.arena_) {}
        ~arena_destroyer() {
            if (arena_) {
                delete arena_;
                arena_ = nullptr;
            }
        }
    };

    template<typename T>
    class arena_ptr : public ptr_base {
        public:
        mutable T *value_ptr_ = nullptr;
        arena_ptr(arena *arena_, T *value_ptr_) : ptr_base{arena_}, value_ptr_(value_ptr_) {}

        public:
        arena_ptr() : ptr_base(nullptr), value_ptr_(nullptr){};

        template<typename Other>
        arena_ptr(Other const &other)
            : ptr_base(other.arena_), value_ptr_(other.value_ptr_) {
        }
        template<typename Other>
        arena_ptr &operator=(Other const &other) {
            arena_ = other.arena_;
            value_ptr_ = other.value_ptr_;
        }

        using element_type = T;
        T *operator->() noexcept { return value_ptr_; }
        T const *operator->() const noexcept { return value_ptr_; }

        T *get() noexcept { return value_ptr_; }
        T const *get() const noexcept { return value_ptr_; }

        friend arena_handle;
        template<typename U, typename... Args>
        friend arena_ptr<U> make_arena_ptr(arena_handle &, Args &&...);
        template<typename U>
        friend arena_ptr<U> make_arena_ptr(arena_handle &, U const &);
    };

    template<typename T>
    arena_ptr<T> make_arena_ptr(arena_handle &handle, T const &other) {
        auto &a = handle.arena_;
        if (!a)
            a = new arena;
        T *value = a->pa.new_object<T>(other);
        return arena_ptr(a, value);
    }

    template<typename T, typename... Args>
    arena_ptr<T> make_arena_ptr(arena_handle &handle, Args &&...args) {
        auto &a = handle.arena_;
        if (!a)
            a = new arena;
        T *value = a->pa.new_object<T>(std::forward<Args>(args)...);
        return arena_ptr(a, value);
    }
}
