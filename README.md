# Intro to onek-parser

*onek-parser* is a minimalistic backtracking recursive descent parser. Its input is kind of C++ compatible EBNF and it generates an AST directly in memory without intermediate code generation.


*"onek"* stands for *"this program/library is made with less than 1000 lines of code"*, which sums up my motivation for writing this library. I previously used [Boost Spirit](https://www.boost.org/doc/libs/1_79_0/libs/spirit/doc/html/spirit/introduction.html), from where I copied some ideas, but with 140,000 lines of code, Spirit is too havy to include in many projects. 

The main difference between onek-parser and other parsers are:

- With onek, left recursion needs to be transformed into right recursion as usual but the program will remain left associative unless special provision are taken. Operator precedence is however handled within the grammar (even though it could be done in an action aswell).
- onek-parser uses generic tokens that can be parameterised with strings. For most purposes the list of tokens does not need to be adapted to each language parsed. Instead of open_curly, open_angled etc... we use open("{"), open("<") etc...

```
                                CURRENT DEVELOPMENT STATUS: WORK IN PROGRESS
```

# The grammar

Here is a grammar for an arithmetic expression evaluator. Note that most of the code is lambda definitions that are alwasy the same for any grammar. For the arithmetic expression grammar below you would only need to write 4 lines of code
```
    auto grammar(onek::scan_state &scn) {
        auto handle = onek::arena_handle();

        // shortcuts for creating terminals
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

        // shortcuts for creating placeholders
        // lambda p creates a normal placeholder
        // lambda f creates a placeholder that does not create a new action root
        auto p = [&handle](const char * name) { return onek::make_arena_ptr<C>(handle, name, onek::TOKEN_FLAG_PLACEHOLDER | onek::TOKEN_FLAG_ACTION_PARENT); };
        auto f = [&handle](const char * name) { return onek::make_arena_ptr<C>(handle, name, onek::TOKEN_FLAG_PLACEHOLDER); };

        // clang-format off
        auto sub_expression =   prod( open("(") >> p("expression") >> close(")")      , "sub");
        auto unsigned_factor =  prod( int_number() | sub_expression                   , "unsigned factor");
        auto factor =           prod((-prefix_op("-") >> unsigned_factor)             , unary_op_action, "factor");
        auto term =             prod( factor >> *(infix_op("*", "/") >> f("term"))    , arithmetic_op_action, "term");
        auto expression =       prod( term >> *(infix_op("+", "-") >> f("expression")), arithmetic_op_action, "expression");
        auto program =          prod( expression >> the_end()                         , "program");
        // clang-format on

        onek::rewire_placeholders<F> fix(program.get());
        return program;
    }
```

# The Abstract Syntax Tree

This grammar applied to the expression "10 - (2 * 10) + 30" will create following AST graph. Note that you can add a name to every production. This name will be shown in the ast graph. The AST can also be executed and in that case, the actions you attach to the nodes will be triggered when their corresponding node is visited.

![example AST](doc/example_ast.png)

As you can see the sub-nodes are flattend agains those nodes that are bound to an action. The actions interpret the code and are easy to write as they travers the children of the action node linearly. For right associative operators they need to traverse those sub-nodes in reverse order.

# The Action Function

In the above grammar, the action `arithmetic_op_action` is bound to expressions and terms. When executing an ast, the actions are triggered and they iterate through the child nodes of their parent node. The user needs to perform these iterations himself, see following example

```
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
```

# EBNF Grammar vs onek-parser
```
   |     EBNF       |    onek-parser       |
   |----------------|----------------------|
   |   a   b        |       a >> b         |
   |  (a   b)+      |     +(a >> b)        |
   |  (a   b)*      |     *(a >> b)        |
   |  (a   b){0,1}  |     -(a >> b)        |
   |  (a   b){3,7}  | (a >> b).repeat(3,7) |
   |   a | b        |       a |  b         |
```

# Summary

To use the *onek-parser* you need to write grammars and actions and very occasionally adapt tokens (for e.g.: some languages allow hyphens in names). You do not need to deal with associativity at the grammar level. If you need to change the code, you can easily do so as this project consists of less than thousand lines of code (not counting test code and error reporting.

