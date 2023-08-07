#pragma once

#include <memory>
#include <memory_resource>

namespace onek {

    namespace detail {

        struct arena {
            static inline constexpr size_t MIN_SIZE = 10000000;
            std::pmr::monotonic_buffer_resource mbr{MIN_SIZE};
            std::pmr::polymorphic_allocator<std::byte> pa{&mbr};
        };

        class ptr_base {
            public:
            detail::arena *arena_ = nullptr;
            explicit ptr_base(detail::arena *arena_) : arena_(arena_) {}
        };
    }

    template<typename> class arena_ptr;

    class arena_handle {
        detail::arena *arena_ = nullptr;
        bool destroy_arena_ = false;
        public:
        arena_handle() = default;
        explicit arena_handle(detail::ptr_base const &p) : arena_(p.arena_){};
        ~arena_handle () {
            if (arena_ && destroy_arena_) {
                delete arena_;
                arena_ = nullptr;
            }
        }
        void set_destroy_arena_on_scope_exit() {destroy_arena_ = true;}

        template<typename T, typename... Args>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, Args &&...);
        template<typename T>
        friend arena_ptr<T> make_arena_ptr(arena_handle &, T const &);
    };

    template<typename T>
    class arena_ptr : public detail::ptr_base {
        mutable T *value_ptr_ = nullptr;
        arena_ptr(detail::arena *arena_, T *value_ptr_) : detail::ptr_base{arena_}, value_ptr_(value_ptr_) {}

        public:
        arena_ptr() : ptr_base(nullptr), value_ptr_(nullptr){};

        arena_ptr(arena_ptr const &other)
            : ptr_base(other.arena_), value_ptr_(other.value_ptr_) {
        }

        using element_type = T;
        // override -> but not *, = or + because those are already
        // used for the grammar to make it look EBNF like.
        T *operator->() noexcept { return value_ptr_; }
        T const *operator->() const noexcept { return value_ptr_; }

        T *get() noexcept { return value_ptr_; }
        T const *get() const noexcept { return value_ptr_; }

        friend arena_handle;
        template<typename U, typename... Args>
        friend arena_ptr<U> make_arena_ptr(arena_handle &handle, Args &&...args);
        template<typename U>
        friend arena_ptr<U> make_arena_ptr(arena_handle &handle, U const &other);
    };

    template<typename T>
    arena_ptr<T> make_arena_ptr(arena_handle &handle, T const &other) {
        auto &a = handle.arena_;
        if (!a)
            a = new detail::arena;
        T *value = a->pa.new_object<T>(other);
        return arena_ptr(a, value);
    }

    template<typename T, typename... Args>
    arena_ptr<T> make_arena_ptr(arena_handle &handle, Args &&...args) {
        auto &a = handle.arena_;
        if (!a)
            a = new detail::arena;
        T *value = a->pa.new_object<T>(std::forward<Args>(args)...);
        return arena_ptr(a, value);
    }
}
