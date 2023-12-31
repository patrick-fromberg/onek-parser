#pragma once

#include "ast.h"
#include "ast_node.h"
#include "scan_state.h"
#include "error_messages.h"
#include <array>
#include <functional>
#include <optional>
#include <iterator>
#include <string_view>
#include <algorithm>
#include <regex>

namespace onek {

    template<typename F>
    class composed_parser;

    template<typename F>
    class terminal_parser;

    template<typename F>
    class parser_base {
        public:
        using N = ast_node<F>;
        using C = composed_parser<F>;
        using A = ast<F>;

        C *parent_ = nullptr;
        bool isRewired = false;
        [[nodiscard]] virtual bool isComposed() const = 0;
        virtual bool parse(A &a, scan_state &scn, N *ast_parent, bool reportErrors) const noexcept = 0;
    };

    template<typename F>
    class terminal_parser : public parser_base<F> {
        public:
        using N = ast_node<F>;
        using A = ast<F>;

        size_t min_repeat_ = 1;
        size_t max_repeat_ = 1;
        const char *delim_ = nullptr;
        const char *name_ = "unknown";
        const token_id token_id_;

        FilterType filters_{nullptr, nullptr, nullptr, nullptr, nullptr};// todo: think of something better
        ushort flags_ = FLAG_NONE;
        using match_function = std::function<std::string_view()>;
        match_function match_;

        public:
        using configuration = F;

        terminal_parser(token_id token_id_, match_function match, ushort flags = FLAG_NONE)
            : name_(token_to_string(token_id_)), token_id_(token_id_), flags_(flags), match_(std::move(match)){};

        terminal_parser(token_id token_id_, scan_state &scn, char const *match, ushort flags = FLAG_NONE)
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

        terminal_parser(token_id token_id_, match_function match, FilterType filters_, ushort flags = FLAG_NONE)
            : name_(token_to_string(token_id_)), token_id_(token_id_), filters_(filters_), flags_(flags), match_(std::move(match)){};

        terminal_parser(token_id token_id_, scan_state &scn, FilterType const &filters, ushort flags = FLAG_NONE)
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

        [[nodiscard]] bool isComposed() const override { return false; }

        bool parse(A &a, scan_state &scn, N *ast_parent, bool reportErrors = false) const noexcept override {
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
                a.add_node(F::default_action, token_id_, token_to_string(token_id_), ast_parent, tokenstr, FLAG_ACTION_PARENT & flags_);
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
                        a.add_node(F::default_action, token_id_, "anonymous", ast_parent, tokenstr, FLAG_ACTION_PARENT & flags_);
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
                    a.add_node(F::default_action, token_id_, "anonymous", ast_parent, tokenstr, FLAG_ACTION_PARENT & flags_);
                    log::scanner_match_success(token_id_, filters_, tokenstr);
                }
                delim = delim_;
            }
            return true;
        }
    };

    template<typename F>
    class composed_parser : public parser_base<F> {
        public:
        using C = composed_parser<F>;
        using A = ast<F>;
        using B = parser_base<F>;
        using N = ast_node<F>;

        size_t min_repeat_ = 1;
        size_t max_repeat_ = 1;
        private:
        const char *delim_ = nullptr;

        using combined_match_ = std::function<bool(B *, B *, A &a, scan_state &scn, N *ast_parent, bool expectFlag)>;
        combined_match_ match_;

        public:
        N::action_function action_ = F::default_action;
        const char *name_ = "unknown";
        ushort flags_ = FLAG_NONE;

        using configuration = F;

        B *left_ = nullptr;
        B *right_ = nullptr;

        composed_parser(C const &) = default;

        composed_parser(combined_match_ match, B *left, B *right)
            : match_(match), left_(left), right_(right) {
        }

        explicit composed_parser(const char *name, ushort flags)
            : name_(name), flags_(flags) {
        }

        [[nodiscard]] bool isComposed() const override { return true; }

        std::optional<A> parse(scan_state &scn, N *ast_parent, bool reportErrors) noexcept {
            A a;
            if (parse(a, scn, ast_parent, reportErrors))
                return a;
            else
                return std::nullopt;
        }

        bool parse(A &a, scan_state &scn, N *ast_parent, bool reportErrors) const noexcept override {

            // saving status in case we have to backtrack
            auto const scn_status = status_saver(scn);
            auto const ast_status = status_saver<ast<F>>(a, ast_parent);//to-do - write deduction guide

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

            N *node = a.add_node(action_, token_id::composed, name_, ast_parent, std::string_view{}, flags_);

            // trying to match this node the minimum required times

            const char *delim = nullptr;
            size_t i = 0;
            for (; i < min_repeat_; ++i) {
                if (delim && !scn.match_delimiter(delim)) {
                    log::scanner_match_error(token_id::delimiter, delim, scn.line_number_, scn.line_begin_, scn.scanner_end_, reportErrors);
                    return log_parser_failure();
                }
                if (!match_(left_, right_, a, scn, node, reportErrors))
                    return log_parser_failure();
                delim = delim_;
            }

            // matching as much as we can until max repeat reached
            // we us a big number instead of infinity, i.e. this src will
            // break if it sees a functions with more than a billion parameters.

            for (; i < max_repeat_; ++i) {
                if (delim) {
                    if (scn.match_delimiter(delim)) {
                        if (!match_(left_, right_, a, scn, node, false)) {
                            scn_status.restore_to(scn);
                            ast_status.restore_to(a);
                            return log_parser_failure();
                        }
                    }
                } else {
                    if (!match_(left_, right_, a, scn, node, false)) {
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

        void copy_bahaviour(C * n) {

            // no placeholder-placeholder
            assert(flags_ & FLAG_PLACEHOLDER && !(n->flags_ & FLAG_PLACEHOLDER));

            // copy all members except the repeat counts, parent node and flags
            delim_ = n->delim_;
            name_ = n->name_;
            match_ = n->match_;
            action_ = n->action_;
            left_ = n->left_;
            right_ = n->right_;
        }
    };
}
