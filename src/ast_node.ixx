module;

#include <functional>
#include <string_view>

export module ast_node;

import token;
import ast_graph;

export namespace onek {

    template<typename F>
    struct ast_node {
        using action_function = std::function<typename F::V(ast_node<F> const &)>;
        action_function action_;
        token_id token_id_ = token_id::error;
        std::string_view tokenstr;
        char const *name_ = "unknown";
        unsigned short flags = FLAG_NONE;
        ast_node *parent_ = nullptr;// todo, use indices instead of pointers
        ast_node *first_child_ = nullptr;
        ast_node *next_sibbling_ = nullptr;
        ast_node *prev_sibbling_ = nullptr;
        ast_node *last_child_ = nullptr;
        ushort id = ++uuid;// only needed for debugging

        bool isTerminal() const noexcept {
            return first_child_ == nullptr;
        }

        F::V action() const noexcept {
            return action_(*this);
        }

        void add_child(ast_node *new_child) {
            if (!first_child_) {
                first_child_ = new_child;
                last_child_ = new_child;
            } else {
                last_child_->next_sibbling_ = new_child;
                new_child->prev_sibbling_ = last_child_;
                last_child_ = new_child;
            }
        }
    };
}
