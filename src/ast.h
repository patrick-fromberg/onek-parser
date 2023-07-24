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

    template<typename Configuration>
    struct ast {
        using ast_node_value = Configuration::ast_node_value;
        using ast_node_t = ast_node<Configuration>;
        std::deque<ast_node<Configuration>> memory_;// Container may not invalidate iterators (vector would crash)

        // todo: remove name parameter and guess it later on
        ast_node_t *add_node(ast_node_t::action_function action, token_id id, const char *name, ast_node_t *parent_node, std::string_view const &tokenstr, unsigned short flags = TOKEN_FLAG_NONE) noexcept {

            // mark nodes that are roots of a bracketed expression. This is only for
            // pretty printing the ast in to a grapviz file.
            auto xxx = parent_node;
            if (id == token_id::open) {//todo: rename id which is different from graph disambituation id
                while (xxx) {
                    if (xxx->flags & TOKEN_FLAG_ACTION_PARENT) {
                        xxx->tokenstr = "(...)";
                        break;
                    }
                    xxx = xxx->parent_;
                }
            }

            // store the node in an arena
            memory_.emplace_back(ast_node_t{action, id, tokenstr, name, flags, parent_node});

            // add it to a parent node that is also bound to an action such that the
            // tree is flattened in a way that actions can be implemented by traversing
            // the children of the action nodes - from left to right to get left associativity
            // and vice versa to get right associativity.
            ast_node_t *added_node = &memory_.back();

            if (id == token_id::open || id == token_id::close)
                return added_node;

            if (id == token_id::composed && !(TOKEN_FLAG_ACTION_PARENT & flags)) {
                if (!parent_node) {
                    added_node->flags |= TOKEN_FLAG_ACTION_PARENT;
                    added_node->name_ = "start";
                }
                return added_node;
            }

            while (parent_node) {
                if (parent_node->flags & TOKEN_FLAG_ACTION_PARENT) {
                    parent_node->add_child(added_node);
                    break;
                }
                parent_node = parent_node->parent_;
            }
            return added_node;
        }

        void print_node(ast_graph &graph, ast_node_t *n) {
            for (ast_node_t *sibbling = n->first_child_; sibbling != nullptr; sibbling = sibbling->next_sibbling_)
                graph.add_edge(n->name_, n->id, sibbling->name_, sibbling->tokenstr, sibbling->id);
            for (ast_node_t *sibbling = n->first_child_; sibbling != nullptr; sibbling = sibbling->next_sibbling_)
                print_node(graph, sibbling);
        }

        void create_ast_graphviz_file(char const *file_name) {
            ast_graph graph(file_name);
            print_node(graph, get_root_node());
        }

        ast_node_t *get_root_node() noexcept {
            ast_node_t &x_ref = memory_.front();
            ast_node_t *x = &x_ref;
            while (x->parent_)
                x = x->parent_;
            return x;
        }

        ast_node_value execute() noexcept {
            ast_node_t const *start = get_root_node();
            ast_node_value result = start->action();
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
    template<typename Configuration>
    class status_saver<ast<Configuration>> {
        using ast_node_t = ast_node<Configuration>;
        const ast_node_t backup_;
        ast_node_t *original_;
        size_t vector_size_;

        public:
        status_saver(ast<Configuration> const &tree, ast_node_t *parent) noexcept
            : backup_(parent ? *parent : ast_node_t{}), original_(parent ? parent : nullptr), vector_size_(tree.memory_.size()) {
        }
        void restore_to(ast<Configuration> &tree) const noexcept {
            if (!tree.memory_.empty() && vector_size_ > 0 && vector_size_ < tree.memory_.size())// to-do - we could use guarantees
                tree.memory_.erase(tree.memory_.begin() + vector_size_);

            if (original_)
                *original_ = backup_;
        }
    };
}
