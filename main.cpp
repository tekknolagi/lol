#include "parser.hpp"

int main() {
    // Parser parser = p_and(p_lit('a'), p_lit('b'));
/*    Parser parser = p_or(
            p_and(p_lit('a'), p_lit('b')),
            p_and(p_lit('c'), p_lit('d'))
        );
        */
    // Parser parser = p_lit("hello");
    // Parser parser = p_between(p_lit('('), p_lit("hello"), p_lit(')'));
    // Parser parser = p_choose("aaaaabc");
    // Parser parser = p_whitespace();
    // Parser parser = p_oneplus(p_choose("abc"));
    // Parser parser = p_oneplus(p_lit("hello"));
    // Parser parser = p_exactly(p_between(p_lit('('), p_lower(), p_lit(')')), 2); 
    // Parser bc = p_and(p_lit('b'), p_lit('c'));
    // Parser parser = p_or(p_lit("hello"), bc);
    // Parser parser = p_or(p_and(p_lit("hello"), bc), p_lit("la"));
    Parser parser = p_hexint();
    ParseResult *result = parser(stdin);
    std::cout << "success? " << result->succeeded() << std::endl;
    std::cout << *result << std::endl;
    return 0;
}
