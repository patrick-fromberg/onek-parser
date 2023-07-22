#pragma once

#include "parser.h"
#include "scan_state.h"
#include "arena_ptr.h"
#include <memory>

namespace onek {

    /*
     * Short variable names:
     *
     *      F: configuration type
     *      C: composed_parser type
     *      T: terminal_parser type
     *      P: parser_ptr polymorphic smart pointer to any parser
     *      p: parser_ptr or arena_ptr to any parser
     *      I: arena_ptr<F>
     *      A: ast<F>
     *      N: ast_node<F>
     *      S: Scan state class
     */

    // return a composed node from two child nodes.
    template<typename P1, typename P2>
    auto operator>>(P1 left, P2 right) {
        using F = typename P1::element_type::configuration;
        using C = composed_parser<F>;
        using P = parser_ptr<F>;
        using S = scan_state;
        using A = ast<F>;
        using N = ast_node<F>;
        auto f = [](P left, P right, A &a, S &s, N *ast_parent, bool expectFlag) -> bool {
            return left->parse(a, s, ast_parent, expectFlag)
                   && right->parse(a, s, ast_parent, expectFlag);
        };
        auto h = arena_handle(left);
        return make_arena_ptr<C>(h, f, left, right);
    }

    // return a composed node from one of two child nodes.
    template<typename P1, typename P2>
    auto operator|(P1 left, P2 right) {
        using F = typename P1::element_type::configuration;
        using C = composed_parser<F>;
        using P = parser_ptr<F>;
        using S = scan_state;
        using A = ast<F>;
        using N = ast_node<F>;
        auto f = [](P left, P right, A &a, S &s, N *ast_parent, bool expectFlag) -> bool {
            if (left->parse(a, s, ast_parent, false))
                return true;
            else
                return right->parse(a, s, ast_parent, expectFlag);
        };
        auto h = arena_handle(left);
        return make_arena_ptr<C>(h, f, left, right);
    }

    long const many = 1000000;

    template<typename P>
    P repeat(P const &n, size_t minrep, size_t maxrep) {
        auto h = arena_handle(n);
        auto p = make_arena_ptr(h, *(n.get()));
        p->repeat(minrep, maxrep);
        return p;
    }

    template<typename P>
    P operator*(P const &p) { return repeat(p, 0, many); }

    template<typename P>
    P operator-(P const &p) { return repeat(p, 0, 1); }

    template<typename P>
    P operator+(P const &p) { return repeat(p, 1, many); }

    template<typename P>
    P clone(P const &n) {
        auto h = arena_handle(n);
        auto p = make_arena_ptr(h, *(n.get()));
        return p;
    }
}
