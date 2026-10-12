#pragma once

#include <string>
#include <vector>

namespace ps {

class CommandLineParser {
public:
    static std::vector<std::string> tokenize(const std::string& line);
};

}  // namespace ps
