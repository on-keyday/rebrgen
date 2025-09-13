/*license*/
#pragma once
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <ebm/extended_binary_module.hpp>

namespace ebmcodegen {
    enum TypeAttribute {
        NONE = 0,
        ARRAY = 0x1,
        PTR = 0x2,
    };
    struct StructField {
        std::string_view name;
        std::string_view type;
        TypeAttribute attr = NONE;
        std::string_view description;  // optional
    };

    struct Struct {
        std::string_view name;
        std::vector<StructField> fields;
    };
    std::map<std::string_view, Struct> make_struct_map();
    std::map<ebm::StatementOp, std::set<std::string_view>> body_subset_StatementBody();
    std::map<ebm::TypeKind, std::set<std::string_view>> body_subset_TypeBody();
    std::map<ebm::ExpressionOp, std::set<std::string_view>> body_subset_ExpressionBody();
}  // namespace ebmcodegen
