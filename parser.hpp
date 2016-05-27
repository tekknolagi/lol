#ifndef PARSER_HPP
#define PARSER_HPP

#include <iostream>
#include <vector>
#include <functional>
#include <unordered_set>

#include <stdio.h>

template<typename T>
void append(std::vector<T> &dst, const std::vector<T> &src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

struct ParseResult {
    std::vector<std::string> result;
    bool success;

    ParseResult() {
        this->success = true;
    }

    ParseResult(char result) {
        this->result.push_back("");
        this->result[0] += result;
        this->success = true;
    }

    ParseResult(const std::string &result) {
        this->result.push_back(result);
        this->success = true;
    }
    
    ParseResult(const ParseResult &result0, const ParseResult &result1) {
        append(this->result, result0.result);
        append(this->result, result1.result);
        this->success = true;
    }

    ParseResult(bool success) {
        this->success = success;
    }

    size_t result_length() const {
        return this->result.size();
    }

    explicit operator bool() const {
        return success;
    }

    std::string string() const {
        std::string res;
        for (const auto &token : result) {
            res += token;
        }
        return res;
    }

    ParseResult operator+(const ParseResult &other) const {
        if (success == false || other.success == false) {
            return failure;
        }

        return ParseResult(*this, other);
    }
    
    void operator+=(const ParseResult &other) {
        if (success == false || other.success == false) {
            success = false;
        }

        append(result, other.result);
    }

    friend std::ostream &operator<<(std::ostream &output,
                                    const ParseResult &result) {
        output << "[";
        for (size_t i = 0; i < result.result.size(); i++) {
            if (i > 0) {
                output << ", ";
            }
            output << result.result[i];
        }
        output << "]";
        return output;
    }

    const static ParseResult failure;
};

const ParseResult ParseResult::failure = ParseResult(false);

typedef std::function<ParseResult (FILE *)> Parser;

/* Parser functions. */

Parser p_empty() {
    return [](FILE *input) {
        return ParseResult();
    };
}

Parser p_any() {
    return [](FILE *input) {
        int result = fgetc(input);
        if (result == EOF) {
            return ParseResult::failure;
        }
        else {
            return ParseResult((char)result);
        }
    };
}

int fpeek(FILE *input) {
    int c = fgetc(input);
    ungetc(c, input);
    return c;
}

Parser p_lit(char c) {
    return [c](FILE *input) {
        int result = fpeek(input);
        if (result == c) {
            fgetc(input);
            return ParseResult(c);
        }
        else {
            return ParseResult::failure;
        }
    };
}

Parser p_lit(const std::string &s) {
    return [s](FILE *input) {
        std::string acc;
        for (size_t i = 0; i < s.length(); i++) {
            int c = fpeek(input);
            if (c != s[i]) { return ParseResult::failure; }
            fgetc(input);
            acc += c;
        }

        return ParseResult(acc);
    };
}

/* Works for both char and string. */
template<typename T>
Parser p_chomp(const T &t) {
    return [t](FILE *input) {
        ParseResult result = p_lit(t);
        if (!result) { return ParseResult::failure; }
        return ParseResult();
    };
}

Parser p_or(const Parser &parser0, const Parser &parser1) {
    return [parser0, parser1](FILE *input) {
        ParseResult result0 = parser0(input);
        if (result0) { return result0; }
        ParseResult result1 = parser1(input);
        if (result1) { return result1; }
        return ParseResult::failure;
    };
}

Parser p_and(const Parser &parser0, const Parser &parser1) {
    return [parser0, parser1](FILE *input) {
        if (input == NULL) {
            return ParseResult::failure;
        }

        ParseResult result0 = parser0(input);
        if (!result0) { return ParseResult::failure; }
        ParseResult result1 = parser1(input);
        if (!result1) { return ParseResult::failure; }
        return result0 + result1;
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

static std::string unique(const std::string &s) {
    std::unordered_set<char> char_set;
    for (char c : s) {
        char_set.insert(c);
    }
    std::string unique_s;
    for (char c : char_set) {
        unique_s += c;
    }
    return unique_s;
}

Parser p_choose(const std::string &chars) {
    if (chars.length() < 1) {
        return p_empty();
    }

    std::string chars_set = unique(chars);
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
    return [parser0, parser1, parser2](FILE *input) {
        if (!parser0(input)) { return ParseResult::failure; }
        ParseResult result = parser1(input);
        if (!result) { return ParseResult::failure; }
        if (!parser2(input)) { return ParseResult::failure; }
        return result;
    };
}

/* TODO: Figure out how to duplicate logic with p_atleast and p_exactly. */

Parser p_atleast(const Parser &parser, size_t n) {
    return [parser, n](FILE *input) {
        ParseResult acc, tempresult;
        while ((tempresult = parser(input))) {
            acc += tempresult;
        }

        if (acc.result_length() >= n) {
            return acc;
        }
        else {
            return ParseResult::failure;
        }
    };
}

Parser p_exactly(const Parser &parser, size_t n) {
    return [parser, n](FILE *input) {
        ParseResult acc, tempresult;
        size_t i = 0;
        while (i++ < n && (tempresult = parser(input))) {
            acc += tempresult;
        }

        if (i == n) {
            return acc;
        }
        else {
            return ParseResult::failure;
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

Parser p_satisfy(bool(*f)(char)) {
    return [f](FILE *input) {
        int result = fpeek(input);
        if (f(result)) {
            fgetc(input);
            return ParseResult((char)result);
        }
        else {
            return ParseResult::failure;
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

Parser p_hexint() {
    return p_and(p_maybe(p_choose("+-")),
                 p_lit("0"),
                 p_or(p_lit("x"), p_lit("X")),
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
