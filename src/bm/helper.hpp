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

    inline bool is_expr(AbstractOp op) {
        switch (op) {
            case AbstractOp::IMMEDIATE_INT:
            case AbstractOp::IMMEDIATE_CHAR:
            case AbstractOp::IMMEDIATE_STRING:
            case AbstractOp::IMMEDIATE_TRUE:
            case AbstractOp::IMMEDIATE_FALSE:
            case AbstractOp::IMMEDIATE_INT64:
            case AbstractOp::IMMEDIATE_TYPE:
            case AbstractOp::BINARY:
            case AbstractOp::UNARY:
            case AbstractOp::CAN_READ:
            case AbstractOp::IS_LITTLE_ENDIAN:
            case AbstractOp::CAST:
            case AbstractOp::CALL_CAST:
            case AbstractOp::ARRAY_SIZE:
            case AbstractOp::ADDRESS_OF:
            case AbstractOp::OPTIONAL_OF:
            case AbstractOp::EMPTY_PTR:
            case AbstractOp::EMPTY_OPTIONAL:
            case AbstractOp::FIELD_AVAILABLE:
            case AbstractOp::INPUT_BYTE_OFFSET:
            case AbstractOp::OUTPUT_BYTE_OFFSET:
            case AbstractOp::REMAIN_BYTES:
            case AbstractOp::PHI:
                return true;
        }
        return false;
    }
}  // namespace rebgn
