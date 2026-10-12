#include "parser.hpp"

#include <cctype>
#include <stdexcept>

namespace ps {

std::vector<std::string> CommandLineParser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaping = false;

    auto flush = [&]() {
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
    };

    for (char ch : line) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\' && !in_single_quote) {
            escaping = true;
            continue;
        }
        if (ch == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }
        if (ch == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) &&
            !in_single_quote && !in_double_quote) {
            flush();
            continue;
        }
        current.push_back(ch);
    }

    if (escaping) {
        current.push_back('\\');
    }
    if (in_single_quote || in_double_quote) {
        throw std::runtime_error("unterminated quote");
    }
    flush();
    return tokens;
}

}  // namespace ps
