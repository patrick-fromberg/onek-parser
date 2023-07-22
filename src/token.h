#pragma once

#include <array>

namespace onek {

    enum class token_id : unsigned short {
        error,
        the_end,
        ident,
        keyword,
        int_number,
        float_number,
        delimiter,
        open,
        close,
        func,
        higher_order_function,
        composed
    };

    constexpr unsigned short TOKEN_FLAG_NONE = 0;
    constexpr unsigned short TOKEN_FLAG_LEFT_ASSOCIATIVE = 1;
    constexpr unsigned short TOKEN_FLAG_RIGHT_ASSOCIATIVE = 2;
    constexpr unsigned short TOKEN_FLAG_DELIM_IN_BETWEEN = 4;
    constexpr unsigned short TOKEN_FLAG_DELIM_AT_END = 8;
    constexpr unsigned short TOKEN_FLAG_ACTION_PARENT = 16;
    constexpr unsigned short TOKEN_FLAG_PREFIX = 32;
    constexpr unsigned short TOKEN_FLAG_INFIX = 64;
    constexpr unsigned short TOKEN_FLAG_POSTFIX = 128;
    constexpr unsigned short TOKEN_FLAG_PLACEHOLDER = 256;

    const char *token_to_string(token_id id) noexcept {
        switch (id) {
            case token_id::error: return "error";
            case token_id::the_end: return "the_end";
            case token_id::ident: return "ident";
            case token_id::keyword: return "keyword";
            case token_id::int_number: return "int_number";
            case token_id::float_number: return "float_number";
            case token_id::delimiter: return "delimiter";
            case token_id::open: return "open";
            case token_id::close: return "close";
            case token_id::func: return "func";
            case token_id::higher_order_function: return "higher_order_function";
            case token_id::composed: return "composed";
        }
        return "token_to_string_error";
    }

    using FilterType = std::array<const char *, 5>;// todo
}