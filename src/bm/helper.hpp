/*license*/
#pragma once
#include "binary_module.hpp"

namespace rebgn {
    inline bool is_declare(AbstractOp op) {
        switch (op) {
            case AbstractOp::DECLARE_FUNCTION:
            case AbstractOp::DECLARE_ENUM:
            case AbstractOp::DECLARE_FORMAT:
            case AbstractOp::DECLARE_UNION:
            case AbstractOp::DECLARE_FIELD:
            case AbstractOp::DECLARE_ENUM_MEMBER:
            case AbstractOp::DECLARE_UNION_MEMBER:
            case AbstractOp::DECLARE_PROGRAM:
            case AbstractOp::DECLARE_STATE:
            case AbstractOp::DECLARE_BIT_FIELD:
            case AbstractOp::DECLARE_PACKED_OPERATION:
            case AbstractOp::DECLARE_PARAMETER:
                return true;
            default:
                return false;
        }
    }
}  // namespace rebgn