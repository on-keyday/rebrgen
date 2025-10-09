/*license*/
#include "transform.hpp"
#include "../convert/helper.hpp"
#include <testutil/timer.h>

namespace ebmgen {

    expected<std::optional<size_t>> sizeof_type(TransformContext& tctx, ebm::TypeRef type) {
        MAYBE(field_type, tctx.type_repository().get(type));
        if (auto size = field_type.body.size()) {
            return size->value();
        }
        else if (auto base_type = field_type.body.base_type(); base_type && !is_nil(*base_type)) {
            return sizeof_type(tctx, *base_type);
        }
        else if (auto members = field_type.body.members(); members) {
            size_t size = 0;
            for (const auto& member : members->container) {
                MAYBE(member_size, sizeof_type(tctx, member));
                if (member_size) {
                    size = std::max(size, *member_size);
                }
                else {
                    return std::nullopt;
                }
            }
            return size;
        }
        else if (auto id = field_type.body.id()) {
            MAYBE(stmt, tctx.statement_repository().get(*id));
            if (auto decl = stmt.body.struct_decl()) {
                if (auto size = decl->size()) {
                    if (size->unit == ebm::SizeUnit::BIT_FIXED) {
                        return size->size()->value();
                    }
                    else if (size->unit == ebm::SizeUnit::BYTE_FIXED) {
                        return size->size()->value() * 8;
                    }
                }
            }
        }
        return std::nullopt;
    }

    expected<void> merge_bit_field(TransformContext& tctx) {
        auto& ctx = tctx.context();
        auto& all_statements = tctx.statement_repository().get_all();
        size_t current_size = all_statements.size();
        for (size_t i = 0; i < current_size; i++) {
            if (auto struct_decl = all_statements[i].body.struct_decl()) {
                auto fields = struct_decl->fields.container;  // copy to avoid relocation
                std::vector<std::pair<size_t, std::optional<size_t>>> sized_fields;
                std::vector<size_t> not_added_index;
                size_t added = 0;
                for (auto [index, field] : fields | std::views::enumerate) {
                    MAYBE(field_stmt, tctx.statement_repository().get(field));
                    if (auto field_decl = field_stmt.body.field_decl()) {
                        MAYBE(size, sizeof_type(tctx, field_decl->field_type));
                        sized_fields.push_back({index, size});
                        added++;
                    }
                    else {
                        not_added_index.push_back(index);
                    }
                }
                std::vector<std::pair<std::optional<size_t>, std::vector<size_t>>> merged;
                for (auto& [index, size] : sized_fields) {
                    if (!size) {
                        merged.push_back({std::nullopt, {index}});
                        continue;
                    }
                    if (merged.empty()) {
                        merged.push_back({size, {index}});
                        continue;
                    }
                    auto& [last_size, last_indexes] = merged.back();
                    if (!last_size) {
                        merged.push_back({size, {index}});
                        continue;
                    }
                    if (*last_size % 8 != 0) {
                        last_indexes.push_back(index);
                        *last_size += *size;
                        continue;
                    }
                    auto is_common = [](size_t size) {
                        return size == 8 || size == 16 || size == 32 || size == 64;
                    };
                    if (!is_common(*last_size)) {
                        last_indexes.push_back(index);
                        *last_size += *size;
                        continue;
                    }
                    if (!is_common(*size) && is_common(*last_size + *size)) {
                        last_indexes.push_back(index);
                        *last_size += *size;
                        continue;
                    }
                    merged.push_back({size, {index}});
                }
                if (merged.size() == added) {
                    continue;
                }
                print_if_verbose("Merging bit fields\n");
                ebm::Block block;
                for (size_t i = 0; i < merged.size(); ++i) {
                    auto& [size, indexes] = merged[i];
                    print_if_verbose("  - Merging ", indexes.size(), " fields");
                    if (size) {
                        print_if_verbose(" with total size ", *size, " bit");
                        if (*size % 8 == 0) {
                            print_if_verbose(" = ", *size / 8, " byte");
                        }
                        print_if_verbose("\n");
                    }
                    else {
                        print_if_verbose(" with unknown size\n");
                    }
                    if (indexes.size() == 1) {
                        append(block, fields[indexes[0]]);
                    }
                    else {
                        ebm::CompositeFieldDecl comp_decl;
                        for (auto& idx : indexes) {
                            append(comp_decl.fields, fields[idx]);
                        }
                        EBMU_UINT_TYPE(composite_type, *size);
                        comp_decl.composite_type = composite_type;
                        ebm::StatementBody body;
                        body.kind = ebm::StatementKind::COMPOSITE_FIELD_DECL;
                        body.composite_field_decl(std::move(comp_decl));
                        EBMA_ADD_STATEMENT(comp_stmt, std::move(body));
                        append(block, comp_stmt);
                    }
                }
                for (auto& not_added : not_added_index) {
                    append(block, fields[not_added]);
                }
                all_statements[i].body.struct_decl()->fields = std::move(block);
            }
        }
        return {};
    }
}  // namespace ebmgen
