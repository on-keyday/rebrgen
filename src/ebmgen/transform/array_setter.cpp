/*license*/
#include "ebm/extended_binary_module.hpp"
#include "../access.hpp"
#include "../converter.hpp"
#include "../visitor/visitor.hpp"

namespace ebmgen {
    struct ArrayLengthInfo {
        const ebm::IOData* write_data = nullptr;
        const ebm::FieldDecl* vector_field = nullptr;
        const ebm::FieldDecl* length_field = nullptr;
        const ebm::Type* vector_type = nullptr;
        const ebm::Type* length_type = nullptr;
    };
    inline std::optional<ArrayLengthInfo> is_setter_target(TransformContext& wctx, const ebm::IOData& data) {
        if (is_nil(data.field)) {
            return std::nullopt;
        }
        auto length_field = access_field<"member.body.id.field_decl">(wctx.context().repository(), data.size.ref());
        if (!length_field) {
            return std::nullopt;
        }
        auto length_type = access_field<"field_type.instance">(wctx.context().repository(), length_field);
        if (!length_type) {
            return std::nullopt;
        }
        // length must be int or uint
        if (length_type->body.kind != ebm::TypeKind::INT &&
            length_type->body.kind != ebm::TypeKind::UINT) {
            return std::nullopt;
        }
        auto array_field = access_field<"member.body.id.field_decl">(wctx.context().repository(), data.target);
        if (!array_field) {
            return std::nullopt;
        }
        auto array_type = access_field<"field_type.instance">(wctx.context().repository(), array_field);
        if (!array_type) {
            return std::nullopt;
        }
        if (array_type->body.kind != ebm::TypeKind::VECTOR) {
            return std::nullopt;
        }
        return ArrayLengthInfo{
            &data,
            array_field,
            length_field,
            array_type,
            length_type,
        };
    }

    struct BranchInfo {
        ebm::StatementRef field_ref;
        ebm::StatementRef assign_ref;
        ebm::StatementRef branch_ref;
    };

    struct PropertySetterDetector {
        TRAVERSAL_VISITOR_BASE_WITHOUT_FUNC(PropertySetterDetector, visitor::BaseVisitor);
    };

    expected<void> derive_array_setter(TransformContext& tctx) {
        std::unordered_map<size_t, ArrayLengthInfo> array_length_setters;
        std::unordered_map<size_t, BranchInfo> branch_info;
        for (auto& stmts : tctx.statement_repository().get_all()) {
            if (auto w = stmts.body.write_data()) {
                if (auto info = is_setter_target(tctx, *w); info) {
                    array_length_setters[get_id(w->field)] = *info;
                }
            }
            // TODO: add generic ast traverser for ebmgen in the future  but currently,
            //       we know exact structure of
            //       derive_property_setter_getter(that is a only function generating ebm::FunctionKind::PROPERTY_SETTER in ebmgen)
            //       generated code
            if (auto func = stmts.body.func_decl(); func && func->kind == ebm::FunctionKind::PROPERTY_SETTER) {
                // assume that the first statement is match_statement
                MAYBE(match_stmt, access_field<"block.container.0.match_statement">(tctx.context().repository(), func->body));
                for (auto& branch : match_stmt.branches.container) {
                    // the second statement is assignment to property field
                    // or match_branch.body is default return. only extract assignment case for now
                    auto assign = access_field<"match_branch.body.block.container.1">(tctx.context().repository(), &branch);
                    if (!assign) {
                        // not assignment, it's ok that this is not a target
                        continue;
                    }
                    // check if the assignment is to vector field with length field
                    auto target_field = access_field<"target.member.body.id.id">(tctx.context().repository(), assign);
                    auto target_field_kind = access_field<"field_decl.field_type.body.kind.optional">(tctx.context().repository(), target_field);
                    if (target_field_kind != ebm::TypeKind::VECTOR) {
                        continue;
                    }
                    branch_info[get_id(*target_field)] = BranchInfo{
                        .field_ref = *target_field,
                        .assign_ref = *assign,
                        .branch_ref = branch,
                    };
                }
            }
        }
        // structure of property setter
        // TODO
        return {};
    }

}  // namespace ebmgen