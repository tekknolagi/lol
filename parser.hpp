#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <vector>
#include <functional>
#include <unordered_set>

#include <stdio.h>

using std::string;
using std::vector;
using std::ostream;
using std::cout;

/*
template<typename T>
void append(std::vector<T> &dst, const std::vector<T> &src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

template<typename T>
void append(std::vector<T> &dst, const T &src) {
    dst.push_back(src);
}
*/

/* Result < Atom
   Atom < String
   Atom < Char
   Result < List<Atom>
   */

class ParseResult {
public:
    ParseResult() : success(true) {}
    ParseResult(bool success) : success(success) {}

    explicit operator bool() const {
        return success;
    }

    bool succeeded() const {
        return success;
    }

    friend ostream &operator<<(ostream &output, const ParseResult *result) {
        return result->print(output);
    }

    friend ostream &operator<<(ostream &output, const ParseResult &result) {
        return result.print(output);
    }

    ParseResult *operator|(ParseResult &b) {
        return union_(&b);
    }

    virtual bool is_empty() const = 0;
    virtual ParseResult *union_(ParseResult *other) const = 0;
    virtual ostream &print(ostream& output) const = 0;

private:
    bool success = true;
};

class ParseFailure : public ParseResult {
public:
    ParseFailure() : ParseResult(false) {}

    bool is_empty() const {
        return true;
    }

    ParseResult *union_(ParseResult *other) const {
        (void)other;
        return new ParseFailure();
    }

    ostream &print(ostream& output) const {
        output << "<ParseFailure>";
        return output;
    }
};

ParseFailure *failure = new ParseFailure();

class ListResult : public ParseResult {
public:
    ListResult() {}
    ListResult(std::initializer_list<ParseResult *> ls) : result(ls) {}
    ListResult(std::vector<ParseResult *> ls) : result(ls) {}

    bool is_empty() const {
        return result.size() == 0;
    }

    ParseResult *union_(ParseResult *other) const {
        ListResult *lr = dynamic_cast<ListResult *>(other);
        if (lr == NULL) {
            ListResult *copy = new ListResult(result);
            copy->push_back(other);
            return copy;
        }
        else {
            vector<ParseResult *> result_copy = result;
            for (const auto &el : lr->result) {
                result_copy.push_back(el);
            }
            return new ListResult(result_copy);
        }
    }

    const vector<ParseResult *> &get_result() const {
        return result;
    }

    void push_back(ParseResult *pr) {
        result.push_back(pr);
    }

    size_t size() const {
        return result.size();
    }

    ostream& print(ostream& output) const {
        output << "[";
        for (size_t i = 0; i < result.size(); i++) {
            if (i > 0) {
                output << ", ";
            }
            output << *result[i];
        }
        output << "]";
        return output;
    }

private:
    vector<ParseResult *> result;
};

template<typename T>
class Result : public ParseResult {
public:
    Result(T t) : result(t) {}

    bool is_empty() const;

    ParseResult *union_(ParseResult *other) const {
        ListResult *lr = dynamic_cast<ListResult *>(other);
        Result<T> *this_copy = new Result<T>(result);
        if (lr == NULL) {
            return new ListResult({this_copy, other});
        }
        else {
            vector<ParseResult *> result_copy;
            result_copy.push_back(this_copy);
            for (const auto &el : lr->get_result()) {
                result_copy.push_back(el);
            }
            return new ListResult(result_copy);
        }
    }

    ostream& print(ostream& output) const {
        output << result;
        return output;
    }

private:
    T result;
};

Result<char> *empty = new Result<char>((char)0);

typedef Result<char> CharResult;
typedef Result<string> StringResult;

template<>
bool CharResult::is_empty() const {
    return result == 0;
}

template<>
bool StringResult::is_empty() const {
    return result == "";
}

typedef std::function<ParseResult *(FILE *)> Parser;

/* Parser functions. */

Parser p_empty() {
    return [](FILE *input) -> ParseResult * {
        return empty;
    };
}

Parser p_any() {
    return [](FILE *input) -> ParseResult * {
        int result = fgetc(input);
        if (result == EOF) {
            return failure;
        }
        else {
            return new CharResult((char)result);
        }
    };
}

int fpeek(FILE *input) {
    int c = fgetc(input);
    ungetc(c, input);
    return c;
}

Parser p_lit(char c) {
    return [c](FILE *input) -> ParseResult * {
        int result = fpeek(input);
        if (result == c) {
            fgetc(input);
            return new CharResult((char)c);
        }
        else {
            return failure;
        }
    };
}

Parser p_lit(const string &s) {
    return [s](FILE *input) -> ParseResult * {
        string acc;
        for (size_t i = 0; i < s.length(); i++) {
            int c = fpeek(input);
            if (c != s[i]) { return failure; }
            fgetc(input);
            acc += c;
        }

        return new StringResult(acc);
    };
}

/* Works for both char and string. */
/*template<typename T>
Parser p_chomp(T &t) {
    return [t](FILE *input) {
        ParseResult *result = p_lit(t);
        if (!*result) { return failure; }
        return new Result<T>();
    };
}*/

Parser p_chomp(const Parser &parser) {
    return [parser](FILE *input) -> ParseResult * {
        ParseResult *result = parser(input);
        if (!*result) { return failure; }
        return empty;
    };
}

Parser p_or(const Parser &parser0, const Parser &parser1) {
    return [parser0, parser1](FILE *input) -> ParseResult * {
        ParseResult *result0 = parser0(input);
        if (*result0) { return result0; }
        ParseResult *result1 = parser1(input);
        if (*result1) { return result1; }
        return failure;
    };
}

Parser p_and(const Parser &parser0, const Parser &parser1) {
    return [parser0, parser1](FILE *input) -> ParseResult * {
        if (input == NULL) {
            return failure;
        }

        ParseResult *result0 = parser0(input);
        if (!*result0) { return failure; }
        ParseResult *result1 = parser1(input);
        if (!*result1) { return failure; }
        if (result0->is_empty() && result1->is_empty()) {
            return result0;
        }
        else if (result0->is_empty()) {
            return result1;
        }
        else if (result1->is_empty()) {
            return result0;
        }
        else {
            return *result0 | *result1;
        }
    };
}

template<typename T>
Parser p_and(const T &parser) {
    return parser;
}

template<typename U, typename... T>
Parser p_and(const U &head, const T &... tail) {
    return p_and(head, p_and(tail...));
}

static string unique(const string &s) {
    std::unordered_set<char> char_set;
    for (char c : s) {
        char_set.insert(c);
    }
    string unique_s;
    for (char c : char_set) {
        unique_s += c;
    }
    return unique_s;
}

Parser p_choose(const string &chars) {
    if (chars.length() < 1) {
        return p_empty();
    }

    string chars_set = unique(chars);
    Parser res = p_lit(chars_set[0]);
    size_t len = chars_set.size();
    for (size_t i = 1; i < len; i++) {
        res = p_or(res, p_lit(chars_set[i]));
    }
    return res;
}

Parser p_between(const Parser &parser0,
                 const Parser &parser1,
                 const Parser &parser2) {
    return [parser0, parser1, parser2](FILE *input) -> ParseResult * {
        if (!*parser0(input)) { return failure; }
        ParseResult *result = parser1(input);
        if (!*result) { return failure; }
        if (!*parser2(input)) { return failure; }
        return result;
    };
}

/* TODO: Figure out how to duplicate logic with p_atleast and p_exactly. */

Parser p_atleast(const Parser &parser, size_t n) {
    return [parser, n](FILE *input) -> ParseResult * {
        ListResult *acc = new ListResult();
        ParseResult *tempresult;
        while (*(tempresult = parser(input))) {
            acc->push_back(tempresult);
        }

        if (acc->size() >= n) {
            return acc;
        }
        else {
            delete acc;
            return failure;
        }
    };
}

Parser p_exactly(const Parser &parser, size_t n) {
    return [parser, n](FILE *input) -> ParseResult * {
        ListResult *acc = new ListResult();
        ParseResult *tempresult;
        size_t i = 0;
        while (i++ < n && *(tempresult = parser(input))) {
            acc->push_back(tempresult);
        }

        if (i == n) {
            return acc;
        }
        else {
            delete acc;
            return failure;
        }
    };
}

Parser p_maybe(const Parser &parser) {
    return p_or(parser, p_empty());
}

Parser p_zeroplus(const Parser &parser) {
    return p_atleast(parser, 0);
}

Parser p_oneplus(const Parser &parser) {
    return p_atleast(parser, 1);
}

template<typename T>
Parser p_satisfy(bool(*f)(T)) {
    return [f](FILE *input) -> ParseResult * {
        int result = fpeek(input);
        if (f(result)) {
            fgetc(input);
            return new Result<T>((T)result);
        }
        else {
            return failure;
        }
    };
}

typedef std::function<void *(void *)> p_apply_func;

/* result should be tagged union */
/* return array or single thing or failure */
/* result should....... */
// Parser p_apply(const Parser &parser, const p_apply_func &f) {
// };

/* Pre-built sets. */

Parser p_whitespace() {
    return p_choose(" \t\n");
}

Parser p_digit() {
    return p_choose("0123456789");
}

Parser p_hexdigit() {
    return p_choose("0123456789ABCDEFabcdef");
}

Parser p_digits() {
    return p_oneplus(p_digit());
}

Parser p_hexdigits() {
    return p_oneplus(p_hexdigit());
}

Parser p_int() {
    return p_and(p_maybe(p_choose("+-")),
                 p_digits());
}

Parser p_group(const Parser &parser) {
    return [parser](FILE *input) -> ParseResult * {
        ParseResult *result = parser(input);
        if (!*result) { return failure; }
        return new ListResult({result});
    };
}

Parser p_hexint() {
    return p_and(p_maybe(p_choose("+-")),
                 p_lit('0'),
                 p_or(p_lit('x'), p_lit('X')),
                 p_hexdigits());
}

Parser p_lower() {
    return p_choose("abcdefghijklmnopqrstuvwxyz");
}

Parser p_upper() {
    return p_choose("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

Parser p_alpha() {
    return p_or(p_lower(), p_upper());
}

Parser p_alphanum() {
    return p_or(p_alpha(), p_digit());
}

#endif
