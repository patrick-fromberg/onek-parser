#include "onek/onek-parser.h"
#include <string_view>
#include <variant>

#define BOOST_TEST_DYN_LINK
//#define BOOST_TEST_MAIN  // in only one cpp file
#define BOOST_TEST_MODULE Calculator
#include <boost/test/unit_test.hpp>
#include <boost/type_index.hpp>

namespace example {

    struct my_parser_configuration;
    using F = my_parser_configuration;
    using C = onek::composed_parser<F>;
    using T = onek::terminal_parser<F>;
    using N = onek::ast_node<F>;

    struct my_parser_configuration {
        using V = std::variant<long, char, double>;

        static inline V default_action(N const &node) noexcept {
            if (node.isTerminal()) {
                switch (node.token_id_) {
                    case onek::token_id::func: return {node.tokenstr.front()};
                    case onek::token_id::int_number: return {atol(node.tokenstr.begin())};
                    case onek::token_id::float_number: return {atof(node.tokenstr.begin())};
                    default: break;
                }
                assert(false);
		return {};
            } else {
                auto child = node.first_child_->next_sibbling_;
                assert(!child || child->token_id_ == onek::token_id::the_end);
                return node.first_child_->action();
            }
        }
    };

    F::V unary_op_action(N const &node) noexcept {
        // if only the operand but not the operator was present, then
        // pass through the value
        if (!node.first_child_->next_sibbling_)
            return node.first_child_->action();

        // assume prefix operator
        N *func = node.first_child_;
        N *operand = func->next_sibbling_;

        // and swap operand and operator if the second
        // is a postfix operator
        if (node.flags & onek::FLAG_POSTFIX)
            std::swap(operand, func);

        // otherwise the first child contains the unary operation
        // which must be applied to the second child
        char c0_value = std::get<char>(func->action());
        auto c1_value = std::get<long>(operand->action());

        auto lambda_ = [](auto op, auto right) { return op == '-' ? -right : right; };
        return {lambda_(c0_value, c1_value)};
    }

    F::V arithmetic_op_action(N const &node) noexcept {
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

    onek::arena_ptr<C> grammar(onek::scan_state &scn) {
        auto handle = onek::arena_handle();

        // shortcuts for creating terminals
        auto the_end = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::the_end, [&scn]() -> std::string_view { return scn.is_end() ? "of the story" : std::string_view{}; }); };
        auto int_number = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::int_number, scn, "^[-]{0,1}[0-9]+"); };
        auto float_number = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::float_number, scn, "^[-]{0,1}[0-9]*[.][0-9]+"); };
        auto ident = [&scn, &handle]() { return onek::make_arena_ptr<T>(handle, onek::token_id::ident, scn, "^[A-Z][A-Z0-9_-]+"); };
        auto open = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::open, scn, onek::FilterType{f...}); };
        auto close = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::close, scn, onek::FilterType{f...}); };
        auto prefix_op = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::func, scn, onek::FilterType{f...}, onek::FLAG_PREFIX); };
        auto infix_op = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::func, scn, onek::FilterType{f...}, onek::FLAG_INFIX); };
        auto postfix_op = [&scn, &handle]<typename... F>(F... f) { return onek::make_arena_ptr<T>(handle, onek::token_id::func, scn, onek::FilterType{f...}, onek::FLAG_POSTFIX); };
        auto func = prefix_op;

        // shortcuts for creating placeholders
        // lambda p creates a normal placeholder, i.e. one that is an action root.
        // lambda f creates a placeholder that does not create a new action root
        auto p = [&handle](const char *name) { return onek::make_arena_ptr<C>(handle, name, onek::FLAG_PLACEHOLDER | onek::FLAG_ACTION_PARENT); };
        auto f = [&handle](const char *name) { return onek::make_arena_ptr<C>(handle, name, onek::FLAG_PLACEHOLDER); };

        // ATTENTION! Note that the placeholders are bound to their original productions by name search.
        // Misspelling the names "expression" and "term" above will result in an error
        // Diagnostic messages for these errors will only be produced if the last parameter of function
        // rewire_placeholders below is 'true'

        // clang-format off
        auto sub_expression =   prod( open("(") >> p("expression") >> close(")")      , "sub");
        auto factor =           prod( int_number() | sub_expression                   , "unsigned factor");
        auto term =             prod( factor >> *(infix_op("*", "/") >> f("term"))    , arithmetic_op_action, "term");
        auto expression =       prod( term >> *(infix_op("+", "-") >> f("expression")), arithmetic_op_action, "expression");
        auto program =          prod( expression >> the_end()                         , "program");
        // clang-format on

        onek::wire_placeholders<F>(program.get(), true);
        return program;
    }
}

void test_expression(std::string_view text, long right_result, char const *ast_graph) {

    auto print_variant_type = [](auto &&value) {
        using T = std::decay_t<decltype(value)>;
        return boost::typeindex::type_id<T>().pretty_name();
    };

    auto scn = onek::scan_state(text);
    auto program = example::grammar(scn);
    auto ast = program->parse(scn, nullptr, true);
    {
        auto handle = onek::arena_handle(program);
        handle.set_destroy_arena_on_scope_exit();
    }
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
}

// clang-format off
BOOST_AUTO_TEST_SUITE(arithmetic_expressions);
BOOST_AUTO_TEST_CASE(simple_expression1)    { test_expression("1 + 2", 3, "ast1.gv"); }
BOOST_AUTO_TEST_CASE(simple_expression2)    { test_expression("1 + 2 - 3", 0, "ast2.gv"); }
BOOST_AUTO_TEST_CASE(simple_expression3)    { test_expression("1 * 2 + 3", 5, "ast3.gv"); }
BOOST_AUTO_TEST_CASE(simple_expression4)    { test_expression("1 + 2 * 3", 7, "ast4.gv"); }
BOOST_AUTO_TEST_CASE(parenthesis5)          { test_expression("(1 + 2) * 3", 9, "ast5.gv"); }
BOOST_AUTO_TEST_CASE(parenthesis6)          { test_expression("2 * (3 + 4)", 14, "ast6.gv"); }
BOOST_AUTO_TEST_CASE(associativity7)        { test_expression("1 - 2 + 3", 2, "ast7.gv"); }
BOOST_AUTO_TEST_CASE(negative_number8)      { test_expression("(1 - -2) * 3", 9, "ast8.gv"); }
BOOST_AUTO_TEST_SUITE_END();
// clang-format on
