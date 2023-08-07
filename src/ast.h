#pragma once

#include "ast_graph.h"
#include "ast_node.h"
#include "error_messages.h"
#include "status_saver_template.h"
#include "token.h"
#include <deque>
#include <functional>
#include <memory>
#include <stack>
#include <string_view>

namespace onek {

    template<typename F>
    struct ast {
        //using V = F::V;
        using N = ast_node<F>;
        std::deque<ast_node<F>> memory_; // Container may not invalidate iterators (vector would crash)

        // todo: remove name parameter and guess it later on
        N *add_node(N::action_function action, token_id id, const char *name, N *parent_node, std::string_view const &tokenstr, unsigned short flags = FLAG_NONE) noexcept {

            // mark nodes that are roots of a bracketed expression. This is only for
            // pretty printing the ast in to a grapviz file.
            auto xxx = parent_node;
            if (id == token_id::open) {//todo: rename id which is different from graph disambiguation id
                while (xxx) {
                    if (xxx->flags & FLAG_ACTION_PARENT) {
                        xxx->tokenstr = "(...)";
                        break;
                    }
                    xxx = xxx->parent_;
                }
            }

            // store the node in an arena
            memory_.emplace_back(N{action, id, tokenstr, name, flags, parent_node});

            // add it to a parent node that is also bound to an action such that the
            // tree is flattened in a way that actions can be implemented by traversing
            // the children of the action nodes - from left to right to get left associativity
            // and vice versa to get right associativity.
            N *added_node = &memory_.back();

            if (id == token_id::open || id == token_id::close)
                return added_node;

            if (id == token_id::composed && !(FLAG_ACTION_PARENT & flags)) {
                if (!parent_node) {
                    added_node->flags |= FLAG_ACTION_PARENT;
                    added_node->name_ = "start";
                }
                return added_node;
            }

            while (parent_node) {
                if (parent_node->flags & FLAG_ACTION_PARENT) {
                    parent_node->add_child(added_node);
                    break;
                }
                parent_node = parent_node->parent_;
            }
            return added_node;
        }

        void print_node(ast_graph &graph, N *n) {
            for (N *child = n->first_child_; child != nullptr; child = child->next_sibbling_)
                graph.add_edge(n->name_, n->id, child->name_, child->tokenstr, child->id);
            for (N *child = n->first_child_; child != nullptr; child = child->next_sibbling_)
                print_node(graph, child);
        }

        void create_ast_graphviz_file(char const *file_name) {
            ast_graph graph(file_name);
            print_node(graph, get_root_node());
        }

        N *get_root_node() noexcept {
            N &x_ref = memory_.front();
            N *x = &x_ref;
            while (x->parent_)
                x = x->parent_;
            return x;
        }

        F::V execute() noexcept {
            N const *start = get_root_node();
            auto result = start->action();
            log::log_result(result);
            return {result};
        }
    };

    // This specialized version of status_saver shall behave exactly the same
    // as the non-specialized version. It exists purely to optimize away expensive
    // copying of the ast::memory_ arena.
    // To understand the following code note, that ast_nodes are values but these
    // values are contained in an intrusive list, hence these values are also entities
    //
    // todo: find another solution. Its not clear that this is a specialisation. Maybe it
    // should simply be an independent class of its own?
    template<typename F>
    class status_saver<ast<F>> {
        using N = ast_node<F>;
        const N backup_;
        N *original_;
        size_t vector_size_;

        public:
        status_saver(ast<F> const &tree, N *parent) noexcept
            : backup_(parent ? *parent : N{}), original_(parent ? parent : nullptr), vector_size_(tree.memory_.size()) {
        }
        void restore_to(ast<F> &tree) const noexcept {
            if (!tree.memory_.empty() && vector_size_ > 0 && vector_size_ < tree.memory_.size())// to-do - we could use guarantees
                tree.memory_.erase(tree.memory_.begin() + vector_size_);

            if (original_)
                *original_ = backup_;
        }
    };
}
