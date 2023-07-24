#pragma once
#include "parser.h"
#include <cstring>

namespace onek {


        template<typename F>
        void rewire_placeholders(parser_base<F> *b, bool produceDiagnostics = false) {
            // todo: do something with parameter produceDiagnostics
            using C = composed_parser<F>;

            // Terminals are not placeholders
            assert(b);
            if (!b->isComposed())
                return;

            // Prevent infinit loop because the parser tree is actually not a tree
            if (b->isRewired)
                return;
            b->isRewired = true;

            // at this point, the parents have not yet been set.
            C *c = reinterpret_cast<C *>(b);
            if (c->left_)
                c->left_->parent_ = c;
            if (c->right_)
                c->right_->parent_ = c;

            // if it is a placeholder, bind it to the original parser
            // todo: try not to reinterpret case
            if (c->flags_ & TOKEN_FLAG_PLACEHOLDER && !c->left_) {
                auto parent = c->parent_;
                assert(!c->right_);
                while (true) {
                    assert(parent);
                    if (parent->name_ == c->name_) {
                        c->copy_bahaviour(parent);
                        break;
                    }
                    parent = parent->parent_;
                }
            }

            // else continue search left and right
            rewire_placeholders(c->left_);
            rewire_placeholders(c->right_);
        }
}
