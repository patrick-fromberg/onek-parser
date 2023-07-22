#pragma once

#include "ast.h"
#include "ast_node.h"
#include "scan_state.h"
#include "error_messages.h"
#include "arena_ptr.h"
#include <array>
#include <functional>
#include <optional>
#include <iterator>
#include <string_view>
#include <algorithm>
#include <regex>

namespace onek {

    template<typename Configuration>
    class parser_base {
        public:
        virtual bool parse(ast<Configuration> &a, scan_state &scn, ast_node<Configuration> *ast_parent, bool reportErrors) const noexcept = 0;
    };

    template<typename Configuration>
    using parser_ptr = arena_ptr<parser_base<Configuration>>;

    template<typename Configuration>
    class terminal_parser : public parser_base<Configuration> {
        size_t min_repeat_ = 1;
        size_t max_repeat_ = 1;
        const char *delim_ = nullptr;
        const char *name_ = "unknown";
        token_id token_id_;
        FilterType filters_{0, 0, 0, 0, 0};// todo: remove this field, capture it insead in the corresponding lambdas
        ushort flags_ = TOKEN_FLAG_NONE;
        using match_function = std::function<std::string_view()>;
        match_function match_;

        public:
        using configuration = Configuration;
        using ptr = std::shared_ptr<terminal_parser>;

        auto repeat(size_t minrep, size_t maxrep) {
            min_repeat_ = minrep;
            max_repeat_ = maxrep;
        }

        terminal_parser(token_id token_id_, match_function const &match, ushort flags = TOKEN_FLAG_NONE)
            : name_(token_to_string(token_id_)), token_id_(token_id_), flags_(flags), match_(match){};

        terminal_parser(token_id token_id_, scan_state &scn, char const *match, ushort flags = TOKEN_FLAG_NONE)
            : name_(token_to_string(token_id_)), token_id_(token_id_), flags_(flags) {

            match_ = [match, &scn]() -> std::string_view {
                auto r = std::regex(match);
                if (scn.p_ >= scn.scanner_end_) return {};
                auto b = std::match_results<std::string_view::const_iterator>{};
                char const *backup = scn.p_;
                while (scn.p_ < scn.scanner_end_ && strchr(" \n\t\r\a", *scn.p_))
                    ++(scn.p_);
                auto x = std::string_view(scn.p_, scn.scanner_end_);
                if (std::regex_search(x.cbegin(), x.cend(), b, r)) {
                    assert(b.size() == 1);
                    auto sub_match = b[0];
                    auto start = scn.p_;
                    auto end = start + sub_match.length();
                    scn.p_ = end;
                    return {start, end};
                }
                scn.p_ = backup;
                return {};
            };
        }

        terminal_parser(token_id token_id_, match_function const &match, FilterType filters_, ushort flags = TOKEN_FLAG_NONE)
            : name_(token_to_string(token_id_)), token_id_(token_id_), filters_(filters_), flags_(flags), match_(match){};

        terminal_parser(token_id token_id_, scan_state &scn, FilterType const &filters, ushort flags = TOKEN_FLAG_NONE)
            : name_(token_to_string(token_id_)), token_id_(token_id_), filters_(filters), flags_(flags) {
            match_ = [&scn, this]() -> std::string_view {
                char const *backup = scn.p_;
                while (scn.p_ < scn.scanner_end_ && strchr(" \n\t\r\a", *scn.p_))
                    ++(scn.p_);
                for (char const *s : filters_) {
                    if (!s) break;
                    size_t len = std::min(long(strlen(s)), long(scn.scanner_end_ - scn.p_));
                    if (len < 1) break;
                    if (!strncmp(scn.p_, s, len)) {
                        auto start = scn.p_;
                        auto end = start + strlen(s);
                        scn.p_ = end;
                        return {start, end};
                    }
                }
                scn.p_ = backup;
                return {};
            };
        }

        bool parse(ast<Configuration> &a, scan_state &scn, ast_node<Configuration> *ast_parent, bool reportErrors = false) const noexcept override {
            // see next overload of function parse for comments

            if (min_repeat_ == 0)
                reportErrors = false;

            const char *delim = nullptr;
            size_t i = 0;
            for (; i < min_repeat_; ++i) {
                if (delim && !scn.match_delimiter(delim)) {
                    log::scanner_match_error(token_id::delimiter, delim, scn.line_number_, scn.line_begin_, scn.scanner_end_, reportErrors);
                    return false;
                }

                std::string_view tokenstr = match_();
                if (tokenstr.empty()) {
                    log::scanner_match_error(token_id_, filters_, scn.line_number_, scn.line_begin_, scn.scanner_end_, reportErrors);
                    return false;
                }
                a.add_node(Configuration::default_action, token_id_, token_to_string(token_id_), ast_parent, tokenstr, TOKEN_FLAG_ACTION_PARENT & flags_);
                log::scanner_match_success(token_id_, filters_, tokenstr);
                delim = delim_;
            }

            for (; i < max_repeat_; ++i) {
                if (delim) {
                    if (scn.match_delimiter(delim)) {
                        std::string_view tokenstr = match_();
                        if (tokenstr.empty()) {
                            log::scanner_match_empty(token_id::delimiter, delim);
                            return false;
                        }
                        a.add_node(Configuration::default_action, token_id_, "anonymous", ast_parent, tokenstr, TOKEN_FLAG_ACTION_PARENT & flags_);
                        log::scanner_match_success(token_id::delimiter, filters_, tokenstr);
                    }
                } else {
                    std::string_view tokenstr = match_();
                    if (tokenstr.empty()) {
                        if (i == 0)// drop composed nodes that have no children but return success
                            ;      // ast_status.restore_to(a);
                        log::scanner_match_empty(token_id_, filters_);
                        break;
                    }
                    a.add_node(Configuration::default_action, token_id_, "anonymous", ast_parent, tokenstr, TOKEN_FLAG_ACTION_PARENT & flags_);
                    log::scanner_match_success(token_id_, filters_, tokenstr);
                }
                delim = delim_;
            }
            return true;
        }
    };

    template<typename Configuration>
    class composed_parser : public parser_base<Configuration> {

        using P = parser_ptr<Configuration>;
        using combined_match_ = std::function<bool(P, P, ast<Configuration> &a, scan_state &scn, ast_node<Configuration> *ast_parent, bool expectFlag)>;
        size_t min_repeat_ = 1;
        size_t max_repeat_ = 1;
        const char *delim_ = nullptr;
        const char *name_ = "anonymous";
        ushort flags_ = TOKEN_FLAG_NONE;
        combined_match_ match_;
        ast_node<Configuration>::action_function action_ = Configuration::default_action;

        public:
        using configuration = Configuration;
        using ptr = arena_ptr<composed_parser>;

        parser_ptr<Configuration> left_;
        parser_ptr<Configuration> right_;

        composed_parser(composed_parser const &) = default;

        composed_parser(combined_match_ match, P left, P right)
            : match_(match), left_(left), right_(right) {
        }

        explicit composed_parser(const char *name)
            : name_(name), flags_(TOKEN_FLAG_PLACEHOLDER) {
        }

        auto repeat(size_t minrep, size_t maxrep) {
            min_repeat_ = minrep;
            max_repeat_ = maxrep;
        }

        ptr debug_name(arena_handle &h, const char *composed_node_name, bool make_action_parent = false) noexcept {
            if (make_action_parent)
                flags_ = TOKEN_FLAG_ACTION_PARENT;
            name_ = composed_node_name;
            return make_arena_ptr(h, *this);
        }

        ptr action(arena_handle &h, ast_node<Configuration>::action_function act) {
            // attach action to node
            flags_ = TOKEN_FLAG_ACTION_PARENT;
            action_ = act;
            return make_arena_ptr(h, *this);
            // todo: check when clone when ref
            //ptr->flags_ = TOKEN_FLAG_ACTION_PARENT;
            //ptr->action_ = act;
            //return ptr;
        }

        std::optional<ast<Configuration>> parse(scan_state &scn, ast_node<Configuration> *ast_parent, bool reportErrors) const noexcept {
            ast<Configuration> a;
            if (parse(a, scn, ast_parent, reportErrors))
                return a;
            else
                return std::nullopt;
        }

        bool parse(ast<Configuration> &a, scan_state &scn, ast_node<Configuration> *ast_parent, bool reportErrors) const noexcept override {

            // saving status in case we have to backtrack
            auto const scn_status = status_saver(scn);
            auto const ast_status = status_saver<ast<Configuration>>(a, ast_parent);//to-do - write deduction guide

            log::log_parser_blockentry(name_, scn.p_, scn.scanner_end_);
            auto log_parser_failure = [&]() {
                scn_status.restore_to(scn);
                ast_status.restore_to(a);
                log::log_parser_blockexit_failure(name_);
                return false;
            };

            // if we are allowed to not match anything,
            // then not matching anything is not an error
            if (min_repeat_ == 0)
                reportErrors = false;

            ast_node<Configuration> *this_ast_node = a.add_node(action_, token_id::composed, name_, ast_parent, std::string_view{}, flags_);

            // trying to match this node the minimum required times

            const char *delim = nullptr;
            size_t i = 0;
            for (; i < min_repeat_; ++i) {
                if (delim && !scn.match_delimiter(delim)) {
                    log::scanner_match_error(token_id::delimiter, delim, scn.line_number_, scn.line_begin_, scn.scanner_end_, reportErrors);
                    return log_parser_failure();
                }
                if (!match_(left_, right_, a, scn, this_ast_node, reportErrors))
                    return log_parser_failure();
                delim = delim_;
            }

            // matching as much as we can until max repeat reached
            // we us a big number instead of infinity, i.e. this src will
            // break if it sees a functions with more than a billion parameters.

            for (; i < max_repeat_; ++i) {
                if (delim) {
                    if (scn.match_delimiter(delim)) {
                        if (!match_(left_, right_, a, scn, this_ast_node, false)) {
                            scn_status.restore_to(scn);
                            ast_status.restore_to(a);
                            return log_parser_failure();
                        }
                    }
                } else {
                    if (!match_(left_, right_, a, scn, this_ast_node, false)) {
                        if (i == 0) {// drop composed nodes that have no children but return success
                            scn_status.restore_to(scn);
                            ast_status.restore_to(a);
                        }
                        break;
                    }
                }
                delim = delim_;
            }
            log::log_parser_blockexit_success(name_);
            return true;
        }

        void copy_bahaviour(ptr const &n) {
            assert(flags_ & TOKEN_FLAG_PLACEHOLDER && !(n->flags_ & TOKEN_FLAG_PLACEHOLDER));
            name_ = n->name_;
            match_ = n->match_;
            action_ = n->action_;
            flags_ = n->flags_;
            left_ = n->left_;
            right_ = n->right_;
        }
    };
}
