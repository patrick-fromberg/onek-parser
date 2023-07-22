#pragma once

#include <boost/circular_buffer.hpp>
#include <iostream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <iterator>
#include <stack>
#include <variant>
#include <algorithm>

namespace onek {

    class log {
        static inline auto cb = boost::circular_buffer<std::stringstream>(30);
        static inline auto ss = std::stringstream{};

        class indentation {
            static inline int indentation_ = 0;
            static inline std::stack<std::pair<char const *, long>> text_;
            static inline long counter = 0;

            public:
            static inline int padding() { return indentation_; }
            static inline void increment() { indentation_ += 4; }
            static inline void decrement() { indentation_ -= 4; }
            static inline auto top() { return text_.top(); }
            static inline void pop() { text_.pop(); }
            static inline void push(char const *s) { text_.emplace(s, ++counter); }
        };

        public:
        static void log_parser_blockentry(const char *name, char const *scn_p_, char const *scn_end_) noexcept {
            auto info = name ? name : "nullptr";
            indentation::push(info);
            ss << std::endl;
            for (size_t i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            ss << "BEGIN COMPOSED: " << info << " '";
            int length = std::min(40L, (scn_end_ - scn_p_));
            std::copy(scn_p_, scn_p_ + length, std::ostreambuf_iterator(ss));
            ss << "...'";
            indentation::increment();
            cb.push_back(std::move(ss));
            ss.clear();
        }

        static void log_parser_blockexit_success(const char *name) noexcept {
            ss << std::endl;
            indentation::decrement();
            for (size_t i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            auto [text, counter] = indentation::top();
            ss << "END COMPOSED: '" << text << '\'';
            indentation::pop();
            cb.push_back(std::move(ss));
            ss.clear();
        }

        static void log_parser_blockexit_failure(const char *name) noexcept {
            auto [text, counter] = indentation::top();
            assert(!strcmp(name, text));
            ss << std::endl;
            indentation::decrement();
            for (size_t i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            ss << "END COMPOSED: '" << text << '\'';
            ss << " BAD *****'";
            indentation::pop();
            cb.push_back(std::move(ss));
            ss.clear();
        }

        static std::ostream &print_filters(FilterType const &filters) noexcept {
            for (auto const &x : filters)
                if (x)
                    ss << '\'' << x << "' ";
            return ss;
        }

        static void scanner_match_success(token_id const &id, FilterType const &filters, std::string_view const &tokenstr) noexcept {
            ss << '\n';
            for (auto i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            ss << "TERMINAL: " << token_to_string(id) << " '" << tokenstr << "'";
            if (filters[0] && filters[0][0] != 0) {
                ss << " with filters: ";
                print_filters(filters);
            }
        }

        static void scanner_match_success(token_id const &id, char const *filter, std::string_view const &tokenstr) noexcept {
            ss << '\n';
            for (auto i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            ss << "TERMINAL: " << token_to_string(id) << " '" << tokenstr << "' with filter: '" << filter << "'";
        }

        // todo: remove paremeter tokenstr
        static void scanner_match_empty(token_id const &id, FilterType const &filters) noexcept {
            ss << '\n';
            for (auto i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            //ss << "TERMINAL: " << token_to_string(id_) << " '";
            ss << "terminal: '" << token_to_string(id) << "' empty match... backtracking";
            if (filters[0] && filters[0][0] != 0) {
                ss << " with filters: ";
                print_filters(filters);
            }
        }

        static void scanner_match_empty(token_id const &id, char const *filter) noexcept {
            ss << '\n';
            for (auto i = 0; i < indentation::padding(); ++i)
                ss << ' ';
            //ss << "TERMINAL: " << token_to_string(id_) << " '";
            ss << "terminal: '" << token_to_string(id) << "' empty match... backtracking";
            ss << " with filters: '" << filter << "'";
        }

        static void scanner_match_error(token_id const &id, FilterType const &filters, size_t line_number, const char *line_begin, const char *scanner_end, bool reportErrors) noexcept {
            if (!reportErrors) {
                scanner_match_empty(id, filters);
                return;
            }

            ss
                << "\nin line " << line_number
                << " error: expected token '" << token_to_string(id);
            if (filters[0]) {
                ss << "' from set: '";
                print_filters(filters);
            }

            ss << "\n    text: '";
            for (auto x = line_begin; *x != '\n' && x != scanner_end; ++x)
                ss << *x;
            ss << "'\n";
        }

        static void scanner_match_error(token_id const &id, char const *filter, size_t line_number, const char *line_begin, const char *scanner_end, bool reportErrors) noexcept {
            if (!reportErrors) {
                scanner_match_empty(id, filter);
                return;
            }
            ss
                << "\nin line " << line_number
                << " error: expected token " << token_to_string(id);
            if (filter)
                ss << "' from set: '" << filter << "'";

            ss << "\n    text: '";
            for (auto x = line_begin; *x != '\n' && x != scanner_end; ++x)
                ss << *x;
            ss << "'\n";
        }
        template<typename AstNodeValue>
        static void log_result(AstNodeValue const &value) noexcept {
            std::visit([](auto &&result) { ss << "\n\nresult: " << result << std::endl; }, value);
        }

        static void print_error_messages() {
            for (auto const &s : cb)
                std::cerr << s.str();
        }
    };

}
