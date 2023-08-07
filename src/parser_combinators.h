#pragma once

#include "parser.h"
#include "ast_node.h"
#include "scan_state.h"
#include "arena_ptr.h"
#include <memory>
#include <type_traits>

namespace onek {

    template<typename L, typename R>
    auto operator>>(L left, R right) {
        using F = typename L::element_type::configuration;
        using B = parser_base<F>;
        using C = composed_parser<F>;
        using S = scan_state;
        using A = ast<F>;
        using N = ast_node<F>;
        auto f = [](B *left, B *right, A &a, S &s, N *ast_parent, bool expectFlag) -> bool {
            return left->parse(a, s, ast_parent, expectFlag)
                   && right->parse(a, s, ast_parent, expectFlag);
        };
        auto h = arena_handle(left);
        return make_arena_ptr<C>(h, f, left.get(), right.get());
    }

    // return a composed node from one of two child nodes.
    template<typename L, typename R>
    auto operator|(L left, R right) {
        using F = typename L::element_type::configuration;
        using B = parser_base<F>;
        using C = composed_parser<F>;
        using S = scan_state;
        using A = ast<F>;
        using N = ast_node<F>;
        auto f = [](B *left, B *right, A &a, S &s, N *ast_parent, bool expectFlag) -> bool {
            if (left->parse(a, s, ast_parent, false))
                return true;
            else
                return right->parse(a, s, ast_parent, expectFlag);
        };
        auto h = arena_handle(left);
        return make_arena_ptr<C>(h, f, left.get(), right.get());
    }

    long const many = 1000000;

    template <typename P>
    P&& prod(P && p, char const * name) {
        p->name_ = name;
        return std::forward<P>(p);
    }

    template <typename P>
    P&& prod(P && p, typename  ast_node<typename P::element_type::configuration>::action_function const& act, char const *name) {
        p->name_ = name;
        p->flags_ = FLAG_ACTION_PARENT;
        p->action_ = act;
        return std::forward<P>(p);
    }

    template<typename P>
    P clone(P const &p) {
        auto h = arena_handle(p);
        return make_arena_ptr(h, *p);
    }

    template<typename P>
    P repeat(P const&p, size_t minrep, size_t maxrep) {
        auto h = arena_handle(p);
        auto pp = make_arena_ptr(h, *(p.get()));
        pp->min_repeat_ = minrep;
        pp->max_repeat_ = maxrep;
        return pp;
    }

    template<typename P>
    P operator*(P const &p) { return repeat(p, 0, many); }

    template<typename P>
    P operator-(P const &p) { return repeat(p, 0, 1); }

    template<typename P>
    P operator+(P const &p) { return repeat(p, 1, many); }

}
