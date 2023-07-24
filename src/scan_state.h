#pragma once

#include <string>
#include <cstring>

namespace onek {

    using scan_ptr = char const *;
    struct scan_state {
        scan_ptr p_;
        scan_ptr scanner_end_;
        scan_ptr line_begin_ = p_;
        size_t line_number_ = 1;// only needed for error reporting

        explicit scan_state(std::string_view text)
            : p_{text.begin()}, scanner_end_{text.end()} {
        }

        [[nodiscard]] bool is_end() const noexcept { return *p_ == 0 || p_ == scanner_end_; }
        [[nodiscard]] bool is_whitespace() const noexcept { return !is_end() && (*p_ == ' ' || *p_ == '\t' || *p_ == '\t'); }
        void advance(size_t size) {
            p_ += size;
            while (is_whitespace()) {
                if (*p_ == '\n') {
                    ++line_number_;
                    line_begin_ = p_;
                }
                ++p_;
            }
        }
        bool match_delimiter(char const *delimiter) {

            if (p_ != nullptr && !is_end() && !strcmp(delimiter, p_)) {
                advance(strlen(delimiter));
                return true;
            }
            return false;
        }
    };
}
