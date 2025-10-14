/*license*/

#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/converter.hpp"
#include "helper.hpp"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace ebmgen {
    struct DerivedTypeInfo {
        ebm::ExpressionRef cond;
        std::optional<ebm::StatementRef> field;  // field or property
    };

    expected<std::optional<ebm::TypeRef>> get_common_type(ConverterContext& ctx, ebm::TypeRef a, ebm::TypeRef b) {
        if (get_id(a) == get_id(b)) {
            return a;
        }
        MAYBE(type_A, ctx.repository().get_type(a));
        MAYBE(type_B, ctx.repository().get_type(b));
        if (type_A.body.kind != type_B.body.kind) {
            auto is_any_range = [&](ebm::Type& t) {
                return t.body.kind == ebm::TypeKind::RANGE && is_nil(*t.body.base_type());
            };
            if (is_any_range(type_A)) {
                return b;
            }
            else if (is_any_range(type_B)) {
                return a;
            }
            return std::nullopt;
        }
        switch (type_A.body.kind) {
            case ebm::TypeKind::INT:
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::FLOAT: {
                auto sizeA = type_A.body.size();
                auto sizeB = type_B.body.size();
                if (sizeA->value() > sizeB->value()) {
                    return a;
                }
                return b;
            }
            default: {
                return std::nullopt;
            }
        }
    }

    expected<std::pair<ebm::ExpressionRef, std::vector<DerivedTypeInfo>>> convert_union_type_to_ebm(ConverterContext& ctx, ast::UnionType& union_type) {
        std::vector<DerivedTypeInfo> cases;
        for (auto& cand : union_type.candidates) {
            DerivedTypeInfo c;
            auto cond_locked = cand->cond.lock();
            if (cond_locked) {
                EBMA_CONVERT_EXPRESSION(expr, cond_locked);
                c.cond = expr;
            }
            auto field = cand->field.lock();
            if (field) {
                EBMA_CONVERT_STATEMENT(stmt, field);
                c.field = stmt;
            }
            cases.push_back(c);
        }
        ebm::ExpressionRef base_cond;
        if (auto cond = union_type.cond.lock()) {
            EBMA_CONVERT_EXPRESSION(expr, cond);
            base_cond = expr;
        }
        return std::make_pair(std::move(base_cond), std::move(cases));
    }

    expected<void> map_field(ConverterContext& ctx, ebm::StatementRef field, ebm::PropertyDecl& decl, auto&& action) {
        if (decl.merge_mode == ebm::MergeMode::STRICT_TYPE) {
            action(field, decl);
        }
        else {
            for (auto& m : decl.members.container) {
                MAYBE(child, ctx.repository().get_statement(m));
                MAYBE(property, child.body.property_decl());
                MAYBE_VOID(ok, map_field(ctx, child.id, property, action));
            }
        }
        return {};
    }

    constexpr auto non_cond_index = 0;
    using PropExprVec = std::vector<std::variant<ebm::Expressions, ebm::PropertyMemberDecl>>;
    struct DetectedTypes {
        std::vector<ebm::TypeRef> detected_types;

        std::unordered_map<std::uint64_t, PropExprVec> merged;
    };

    expected<DetectedTypes> detect_all_types(ConverterContext& ctx, std::vector<DerivedTypeInfo>& cases) {
        DetectedTypes result;
        // detect all types before merge
        for (auto& c : cases) {
            if (c.field) {
                MAYBE(field, ctx.repository().get_statement(*c.field));
                if (auto decl = field.body.field_decl()) {
                    if (result.merged.emplace(get_id(decl->field_type), PropExprVec{}).second) {
                        result.detected_types.push_back(decl->field_type);
                    }
                }
                else if (auto decl = field.body.property_decl()) {
                    MAYBE_VOID(ok, map_field(ctx, field.id, *decl, [&](ebm::StatementRef, ebm::PropertyDecl& decl) {
                                   if (result.merged.emplace(get_id(decl.property_type), PropExprVec{}).second) {
                                       result.detected_types.push_back(decl.property_type);
                                   }
                               }));
                }
                else {
                    return unexpect_error("Only field or property can be used in union type");
                }
            }
        }
        return result;
    }

    expected<void> merge_fields(ConverterContext& ctx, std::vector<DerivedTypeInfo>& cases, std::unordered_map<std::uint64_t, PropExprVec>& merged) {
        auto add_only_cond = [&](PropExprVec& p, ebm::ExpressionRef cond) {
            if (p.size() && p.back().index() == non_cond_index) {
                append(std::get<non_cond_index>(p.back()), cond);
            }
            else {
                p.push_back(ebm::Expressions{.len = *varint(1), .container = {cond}});
            }
        };
        for (auto& c : cases) {
            if (c.field) {
                MAYBE(field, ctx.repository().get_statement(*c.field));
                std::unordered_set<std::uint64_t> added;
                if (auto decl = field.body.field_decl()) {
                    auto id = get_id(decl->field_type);
                    added.insert(id);
                    merged[id].push_back(ebm::PropertyMemberDecl{
                        .condition = c.cond,
                        .field = *c.field,
                    });
                }
                else if (auto decl = field.body.property_decl()) {
                    auto id = get_id(decl->property_type);
                    MAYBE_VOID(ok, map_field(ctx, field.id, *decl, [&](ebm::StatementRef field, ebm::PropertyDecl& decl) {
                                   added.insert(id);
                                   merged[id].push_back(ebm::PropertyMemberDecl{
                                       .condition = c.cond,
                                       .field = field,
                                   });
                               }));
                }
                else {
                    return unexpect_error("Only field or property can be used in union type");
                }
                for (auto& not_added : merged) {
                    if (added.contains(not_added.first)) {
                        continue;
                    }
                    add_only_cond(not_added.second, c.cond);
                }
            }
            else {
                for (auto& a : merged) {
                    add_only_cond(a.second, c.cond);
                }
            }
        }
        return {};
    }

    expected<void> strict_merge(ConverterContext& ctx, ebm::PropertyDecl& derive, ebm::ExpressionRef base_cond, ebm::TypeRef type, PropExprVec& vec) {
        derive.merge_mode = ebm::MergeMode::STRICT_TYPE;
        derive.property_type = type;
        derive.cond(base_cond);
        for (auto& member : vec) {
            if (auto got = std::get_if<non_cond_index>(&member)) {
                ebm::TypeRef common_type;
                for (auto& expr : got->container) {
                    MAYBE(got, ctx.repository().get_expression(expr));
                    if (is_nil(common_type)) {
                        common_type = got.body.type;
                    }
                    else {
                        MAYBE(ct, get_common_type(ctx, common_type, got.body.type));
                        if (!ct) {
                            MAYBE(prev, ctx.repository().get_type(common_type));
                            MAYBE(current, ctx.repository().get_type(got.body.type));
                            return unexpect_error("cannot get common type: {} vs {}", to_string(prev.body.kind), to_string(current.body.kind));
                        }
                        common_type = *ct;
                    }
                }
                if (got->container.size() == 0) {
                    return unexpect_error("This is a bug: empty condition in strict type merge");
                }
                ebm::PropertyMemberDecl m{};
                if (got->container.size() == 1) {
                    m.condition = got->container[0];
                }
                else {
                    EBM_OR_COND(or_, common_type, std::move(*got));
                    m.condition = or_;
                }
                EBM_PROPERTY_MEMBER_DECL(member_ref, std::move(m));
                append(derive.members, member_ref);
            }
            else {
                EBM_PROPERTY_MEMBER_DECL(member_ref, std::move(std::get<1>(member)));
                append(derive.members, member_ref);
            }
        }
        return {};
    }

    using IndexCluster = std::vector<std::pair<std::vector<size_t>, std::unordered_set<size_t>>>;

    expected<IndexCluster> clustering_properties(ConverterContext& ctx, std::vector<ebm::PropertyDecl>& properties) {
        std::map<std::uint64_t, std::vector<size_t>> edge;
        // insertion order is important, so use both vector and set
        std::vector<std::pair<std::vector<size_t>, std::unordered_set<size_t>>> cluster;
        for (size_t i = 0; i < properties.size(); i++) {
            edge[i].emplace_back();
            for (size_t j = i + 1; j < properties.size(); j++) {
                MAYBE(common, get_common_type(ctx, properties[i].property_type, properties[j].property_type));
                if (common) {
                    edge[i].push_back(j);
                }
            }
        }
        for (size_t i = 0; i < properties.size(); i++) {
            auto found = edge.find(i);
            if (found == edge.end()) {
                continue;
            }
            bool has_cluster = false;
            for (auto& c : cluster) {
                if (!c.second.contains(i)) {
                    continue;
                }
                has_cluster = true;
                for (auto& e : found->second) {
                    if (c.second.insert(e).second) {
                        c.first.push_back(e);
                    }
                }
                break;
            }
            if (has_cluster) {
                continue;
            }
            cluster.push_back({{i}, {i}});
            for (auto& e : found->second) {
                if (cluster.back().second.insert(e).second) {
                    cluster.back().first.push_back(e);
                }
            }
        }
        return cluster;
    }

    auto derive_variant(ConverterContext& ctx, auto&& arr, ebm::TypeRef common_type, auto&& get_type) -> expected<ebm::TypeRef> {
        ebm::Types types;
        for (auto& t : arr) {
            append(types, get_type(t));
        }
        ebm::TypeBody body;
        body.kind = ebm::TypeKind::VARIANT;
        body.members(std::move(types));
        body.common_type(common_type);
        EBMA_ADD_TYPE(c_type, std::move(body));
        return c_type;
    }

    expected<std::vector<ebm::PropertyDecl>> common_merge(ConverterContext& ctx, ebm::PropertyDecl& derive, IndexCluster& cluster, std::vector<ebm::PropertyDecl>& properties) {
        std::vector<ebm::PropertyDecl> final_props;

        for (auto& c : cluster) {
            if (c.first.size() == 1) {
                final_props.push_back(std::move(properties[c.first[0]]));
                continue;
            }
            ebm::TypeRef common_type;
            for (auto& index : c.first) {
                if (is_nil(common_type)) {
                    common_type = properties[index].property_type;
                    continue;
                }
                MAYBE(detect, get_common_type(ctx, common_type, properties[index].property_type));
                if (!detect) {
                    return unexpect_error("no common type found for clustered types; THIS IS BUG!");
                }
                common_type = *detect;
            }
            auto c_type = derive_variant(ctx, c.first, common_type, [&](size_t index) {
                return properties[index].property_type;
            });
            if (!c_type) {
                return unexpect_error(std::move(c_type.error()));
            }
            ebm::PropertyDecl prop;
            prop.name = derive.name;
            prop.property_type = *c_type;
            prop.parent_format = derive.parent_format;
            prop.merge_mode = ebm::MergeMode::COMMON_TYPE;
            for (auto& index : c.first) {
                EBM_PROPERTY_DECL(p, std::move(properties[index]));
                append(prop.members, p);
            }
            final_props.push_back(std::move(prop));
        }
        return final_props;
    }

    expected<void> uncommon_merge(ConverterContext& ctx, ebm::PropertyDecl& derive, std::vector<ebm::PropertyDecl>& final_props) {
        auto c_type = derive_variant(ctx, final_props, {}, [&](auto& prop) {
            return prop.property_type;
        });
        if (!c_type) {
            return unexpect_error(std::move(c_type.error()));
        }
        ebm::PropertyDecl prop;
        prop.name = derive.name;
        prop.property_type = *c_type;
        prop.parent_format = derive.parent_format;
        prop.merge_mode = ebm::MergeMode::UNCOMMON_TYPE;
        for (auto& f : final_props) {
            EBM_PROPERTY_DECL(pd, std::move(f));
            append(prop.members, pd);
        }
        derive = std::move(prop);
        return {};
    }

    expected<void> derive_property_type(ConverterContext& ctx, ebm::PropertyDecl& derive, ast::UnionType& union_type) {
        MAYBE(union_data, convert_union_type_to_ebm(ctx, union_type));
        auto [base_cond, cases] = std::move(union_data);
        MAYBE(all_type, detect_all_types(ctx, cases));
        auto [detected_types, merged_fields] = std::move(all_type);
        MAYBE_VOID(merge_field, merge_fields(ctx, cases, merged_fields));
        print_if_verbose("Merged ", merged_fields.size(), " types for property\n");
        if (merged_fields.size() == 1) {  // single strict type
            MAYBE_VOID(s, strict_merge(ctx, derive, base_cond, ebm::TypeRef{merged_fields.begin()->first}, merged_fields.begin()->second));
            return {};
        }
        std::vector<ebm::PropertyDecl> properties;
        for (auto& ty : detected_types) {
            ebm::PropertyDecl prop;
            prop.name = derive.name;
            prop.parent_format = derive.parent_format;
            MAYBE_VOID(s, strict_merge(ctx, prop, base_cond, ty, merged_fields[get_id(ty)]));
            properties.push_back(std::move(prop));
        }
        MAYBE(cluster, clustering_properties(ctx, properties));
        MAYBE(final_props, common_merge(ctx, derive, cluster, properties));
        if (final_props.size() == 1) {  // all types are merged into single common type
            derive = std::move(final_props[0]);
        }
        else {
            MAYBE_VOID(prop, uncommon_merge(ctx, derive, final_props));
        }
        return {};
    }

    expected<ebm::StatementBody> StatementConverter::convert_property_decl(const std::shared_ptr<ast::Field>& node) {
        ebm::StatementBody body;
        MAYBE(union_type, ast::as<ast::UnionType>(node->field_type));
        body.kind = ebm::StatementKind::PROPERTY_DECL;
        ebm::PropertyDecl prop_decl;
        EBMA_ADD_IDENTIFIER(field_name_ref, node->ident->ident);
        prop_decl.name = field_name_ref;

        if (auto parent_member = node->belong.lock()) {
            EBMA_CONVERT_STATEMENT(statement_ref, parent_member);
            prop_decl.parent_format = statement_ref;
        }

        MAYBE_VOID(derived, derive_property_type(ctx, prop_decl, union_type));

        body.property_decl(std::move(prop_decl));
        ctx.state().cache_type(node->field_type, prop_decl.property_type);
        return body;
    }
}  // namespace ebmgen
