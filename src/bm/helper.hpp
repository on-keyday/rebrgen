/*license*/
#pragma once
#include "binary_module.hpp"
#include <format>

namespace rebgn {

    inline bool is_declare(AbstractOp op) {
        switch (op) {
            case AbstractOp::DECLARE_FUNCTION:
            case AbstractOp::DECLARE_ENUM:
            case AbstractOp::DECLARE_FORMAT:
            case AbstractOp::DECLARE_UNION:
            case AbstractOp::DECLARE_UNION_MEMBER:
            case AbstractOp::DECLARE_PROGRAM:
            case AbstractOp::DECLARE_STATE:
            case AbstractOp::DECLARE_BIT_FIELD:
                return true;
            default:
                return false;
        }
    }

    inline std::string storage_key(const Storages& s) {
        std::string key;
        for (auto& storage : s.storages) {
            key += to_string(storage.type);
            if (auto ref = storage.ref()) {
                key += std::format("r{}", ref.value().value());
            }
            if (auto size = storage.size()) {
                key += std::format("s{}", size.value().value());
            }
        }
        return key;
    }
}  // namespace rebgn