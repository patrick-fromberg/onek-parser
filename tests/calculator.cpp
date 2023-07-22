#include "onek/onek-parser.h"
#include <string_view>
#include <variant>

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN// in only one cpp file
#define BOOST_TEST_MODULE Calculator
#include <boost/test/unit_test.hpp>
#include <boost/type_index.hpp>

namespace example {

    struct my_parser_configuration;
    using F = my_parser_configuration;
    using V = std::variant<long, char, double>;
    using C = onek::composed_parser<F>;
    using T = onek::terminal_parser<F>;
    using N = onek::ast_node<F>;
    using S = onek::scan_state;

    struct my_parser_configuration {
        using ast_node_value = V;

        static inline ast_node_value default_action(onek::ast_node<my_parser_configuration> const &node) noexcept {
            auto lonely_child = node.first_child_;
            if (lonely_child) {
                if (lonely_child->next_sibbling_)
                    if (lonely_child->token_id_ == onek::token_id::open)
                        return lonely_child->next_sibbling_->action();
                return lonely_child->action();
            }

            // clang-format off
            switch(node.token_id_) {
                case onek::token_id::func: return { node.tokenstr.front() };
                case onek::token_id::int_number: return {atol(node.tokenstr.begin())};
                case onek::token_id::float_number: return {atof(node.tokenstr.begin())};
                default: assert(false);
            };
            // clang-format on

            return {};
        }
    };

    V unary_op_action(N const &node) noexcept {
        // if only the operand but not the operator was present, then
        // pass through the value
        if (!node.first_child_->next_sibbling_)
            return node.first_child_->action();

        // assume prefix operator
        N *func = node.first_child_;
        N *operand = func->next_sibbling_;

        // and swap operand and operator if the second
        // is a postfix operator
        if (node.flags & onek::TOKEN_FLAG_POSTFIX)
            std::swap(operand, func);

        // otherwise the first child contains the unary operation
        // which must be applied to the second child
        char c0_value = std::get<char>(func->action());
        auto c1_value = std::get<long>(operand->action());

        auto lambda_ = [](auto op, auto right) { return op == '-' ? -right : right; };
        return {lambda_(c0_value, c1_value)};
    }

    V arithmetic_op_action(N const &node) noexcept {
        N *left = node.first_child_;
        long left_value = std::get<long>(left->action());
        long sum = left_value;

        while (true) {
            N *middle = left->next_sibbling_;
            if (!middle)
                break;
            N *right = middle->next_sibbling_;
            if (!right)
                std::cerr << "programming error: expecting another sibbling child after:"
                          << middle->name_ << std::endl;
            long right_value = std::get<long>(right->action());
            char op = std::get<char>(middle->action());
            if (op == '+')
                sum += right_value;
            else if (op == '-')
                sum -= right_value;
            else if (op == '*')
                sum *= right_value;
            else if (op == '/')
                sum /= right_value;
            left = right;
        }
        return {sum};
    }

    auto grammar(onek::scan_state &scn) {
        auto handle = onek::arena_handle();

        auto the_end = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::the_end, [&scn]() -> std::string_view { return scn.is_end() ? "of the story" : std::string_view{}; }); };
        auto int_number = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::int_number, scn, "^[0-9]+"); };
        auto float_number = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::float_number, scn, "^[0-9]*[.][0-9]+"); };
        auto ident = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::ident, scn, "^[A-Z][A-Z0-9_-]+"); };
        auto open = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::open, scn, onek::FilterType{f...}); };
        auto close = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::close, scn, onek::FilterType{f...}); };
        auto prefix_op = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::func, scn, onek::FilterType{f...}, onek::TOKEN_FLAG_PREFIX); };
        auto infix_op = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::func, scn, onek::FilterType{f...}, onek::TOKEN_FLAG_INFIX); };
        auto postfix_op = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::func, scn, onek::FilterType{f...}, onek::TOKEN_FLAG_POSTFIX); };
        auto func = prefix_op;

        // we need placeholder parsers for the recursive production rules and they need to be
        // held by a weak pointer such that no circular referencing happens at the level
        // of the shared pointers.
        auto p_expression = onek::make_arena_ptr<C>(handle, "expression");
        auto p_expression_x = onek::make_arena_ptr<C>(handle, "expression_x");
        auto p_term = onek::make_arena_ptr<C>(handle, "term");
        auto p_term_x = onek::make_arena_ptr<C>(handle, "term_x");

        // Every rule constructs a src by combining smaller parsers
        // clang-format off
        auto sub_expression =   ( open("(") >> p_expression >> close(")")             )->debug_name(handle, "sub_expression");
        auto unsigned_factor =  ( int_number() | float_number() | sub_expression      )->debug_name(handle, "unsigned factor");
        auto factor =           (-prefix_op("-") >> unsigned_factor                   )->action(handle, unary_op_action)->debug_name(handle, "factor");
        auto term_x =           ( factor >> *(infix_op("*", "/") >> p_term_x)         )->debug_name(handle,"term_x");
        auto term =             ( clone(term_x)                                       )->action(handle, arithmetic_op_action)->debug_name(handle, "term");
        auto expression_x =     ( term >> *(infix_op("+", "-") >> p_expression_x)     )->debug_name(handle,"expression_x");
        auto expression =       ( clone(expression_x)                                 )->action(handle, arithmetic_op_action)->debug_name(handle, "expression");
        auto program =          ( expression >> the_end()                             )->debug_name(handle, "program", true);
        // clang-format on

        // bind the placeholder parsers to their corresponding parsers
        p_expression->copy_bahaviour(expression);
        p_expression_x->copy_bahaviour(expression_x);
        p_term->copy_bahaviour(term);
        p_term_x->copy_bahaviour(term_x);
        return program;
    }
}

void test_simple_expression(std::string_view text, long right_result, char const *ast_graph) {

    auto print_variant_type = [](auto &&value) {
        using T = std::decay_t<decltype(value)>;
        return boost::typeindex::type_id<T>().pretty_name();
    };

    auto scn = onek::scan_state(text);
    auto program = example::grammar(scn);
    auto ast = program->parse(scn, nullptr, true);
    if (!ast) {
        BOOST_CHECK_MESSAGE(false, std::string(text) + " did not compile");
        onek::log::print_error_messages();
    } else {
        ast->create_ast_graphviz_file(ast_graph);

        auto result = ast->execute();
        BOOST_CHECK_MESSAGE(holds_alternative<long>(result), "result of " + std::string(text)
                                                                 + " should be of type long is " + std::visit(print_variant_type, result));
        auto long_result = get<long>(result);
        if (holds_alternative<long>(result))
            BOOST_CHECK_MESSAGE(long_result == right_result, "result of " + std::string(text) + " should be "
                                                                 + std::to_string(right_result) + " is " + std::to_string(long_result));
    }
    auto handle = onek::arena_handle(program);
    auto destroyer = onek::arena_destroyer(handle);
}

BOOST_AUTO_TEST_CASE(simple_expression) {
    test_simple_expression("1 + 1", 2, "ast1.gv");
    test_simple_expression("1 - 2 + 3", 2, "ast2.gv");
    test_simple_expression("2 * 3", 6, "ast3.gv");
    test_simple_expression("2 + 3 * 7", 23, "ast4.gv");
    test_simple_expression("(2 + 3) * 7", 35, "ast5.gv");
    test_simple_expression("(2 + -3) * 7", -7, "ast6.gv");
    test_simple_expression("(2 -  3) * -7", 7, "ast7.gv");
    test_simple_expression("10 - (2 * 10) + 30", 20, "ast8.gv");
}
