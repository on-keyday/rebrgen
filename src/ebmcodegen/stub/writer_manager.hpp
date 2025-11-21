/*license*/
#pragma once
#include <vector>
#include "binary/writer.h"
#include "code/code_writer.h"
#include <helper/defer.h>

namespace ebmcodegen {
    template <class CodeWriter>
    struct WriterManager {
        futils::code::CodeWriter<futils::binary::writer&>& root;
        std::vector<CodeWriter> tmp_writers;
        [[nodiscard]] auto add_writer() {
            tmp_writers.emplace_back();
            return futils::helper::defer([&]() {
                tmp_writers.pop_back();
            });
        }
        CodeWriter* get_writer() {
            if (tmp_writers.empty()) {
                return nullptr;
            }
            return &tmp_writers.back();
        }
    };
}  // namespace ebmcodegen