#pragma once

namespace onek {

    template<typename T>
    //requires copy constructor from T
    //requires void restore_to(T &) method
    class status_saver {
        T const status_;

        public:
        status_saver(T const &x) noexcept
            : status_(x) {
        }
        void restore_to(T &x) const noexcept {
            x = status_;
        }
    };

}
