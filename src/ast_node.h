#pragma once

#include "ast_node.h"
#include "token.h"
#include "ast_graph.h"
#include <functional>
#include <variant>

namespace onek {

    template<typename Configuration>
    struct ast_node {
        using ast_node_value = Configuration::ast_node_value;
        using action_function = std::function<ast_node_value(ast_node<Configuration> const &)>;
        action_function action_;
        token_id token_id_ = token_id::error;
        std::string_view tokenstr;
        char const *name_ = "unknown";
        unsigned short flags = TOKEN_FLAG_NONE;
        ast_node *parent_ = nullptr;// todo, use indices instead of pointers
        ast_node *first_child_ = nullptr;
        ast_node *next_sibbling_ = nullptr;// todo, rename
        ast_node *last_child_ = nullptr;
        ushort id = ++uuid;// only needed for debugging

        ast_node_value action() const noexcept {
            return action_(*this);
        }

        void add_child(ast_node *new_child) {
            if (!first_child_)
                first_child_ = new_child;
            else
                last_child_->next_sibbling_ = new_child;
            last_child_ = new_child;
        }
    };
}
