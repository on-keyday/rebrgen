/*license*/
#pragma once
#include <string>

namespace ebmcodegen::util {
    // remove top level brace
    inline std::string tidy_condition_brace(std::string&& brace) {
        if (brace.starts_with("(") && brace.ends_with(")")) {
            // check it is actually a brace
            // for example, we may encounter (1 + 1) + (2 + 2)
            // so ensure that brace is actually (<expr>)
            size_t level = 0;
            for (size_t i = 0; i < brace.size(); ++i) {
                if (brace[i] == '(') {
                    ++level;
                }
                else if (brace[i] == ')') {
                    --level;
                }
                if (level == 0) {
                    if (i == brace.size() - 1) {
                        // the last char is the matching ')'
                        brace = brace.substr(1, brace.size() - 2);
                        return std::move(brace);
                    }
                    break;  // not a top-level brace
                }
            }
        }
        return std::move(brace);
    }
}  // namespace ebmcodegen::util
