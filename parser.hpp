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

    bool is_empty() const {
        return result_length() == 0;
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

Parser p_lit(char c) {
    return [c](FILE *input) {
        int result = fgetc(input);
        ungetc(result, input);
        if (result == c) {
            fgetc(input);
            return ParseResult(c);
        }
        else {
            return ParseResult::failure;
        }
    };
}

/* Forward declaration for use in string literal. May change string literal to 
 * return whole strings instead of many characters. */
Parser p_and(const Parser &parser0, const Parser &parser1);

Parser p_lit(const std::string &s) {
    Parser res = p_empty();
    size_t len = s.size();
    for (size_t i = 0; i < len; i++) {
        res = p_and(res, p_lit(s[i]));
    }
    return res;
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

static inline int goto_pos(FILE *fp, long pos) {
    return fseek(fp, pos, SEEK_SET);
}

Parser p_and(const Parser &parser0, const Parser &parser1) {
    return [parser0, parser1](FILE *input) {
        if (input == NULL) {
            return ParseResult::failure;
        }

        long int pos = ftell(input);
        ParseResult result0 = parser0(input);
        if (!result0) { goto_pos(input, pos); return ParseResult::failure; }
        ParseResult result1 = parser1(input);
        if (!result1) { goto_pos(input, pos); return ParseResult::failure; }
        return result0 + result1;
    };
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
        while ((tempresult = parser(input))) {
            acc += tempresult;
        }

        if (acc.result_length() == n) {
            return acc;
        }
        else {
            return ParseResult::failure;
        }
    };
}

Parser p_zeroplus(const Parser &parser) {
    return p_atleast(parser, 0);
}

Parser p_oneplus(const Parser &parser) {
    return p_atleast(parser, 1);
}

/* Pre-built sets. */

Parser p_whitespace() {
    return p_choose(" \t\n");
}

Parser p_digit() {
    return p_choose("0123456789");
}

Parser p_lower() {
    return p_choose("abcdefghijklmnopqrstuvwxyz");
}

Parser p_upper() {
    return p_choose("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

#endif
