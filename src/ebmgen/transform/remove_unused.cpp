/*license*/
#include "transform.hpp"
#include <set>
#include <testutil/timer.h>

namespace ebmgen {
    template <class B>
    concept has_body_kind = requires(B b) {
        { b.body.kind };
    };

    expected<void> remove_unused_object(TransformContext& ctx) {
        MAYBE(max_id, ctx.max_id());
        std::map<size_t, std::vector<ebm::AnyRef>> used_refs;
        auto trial = [&] -> expected<size_t> {
            used_refs.clear();
            auto map_to = [&](const auto& vec) {
                for (const auto& item : vec) {
                    item.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                        if constexpr (AnyRef<decltype(val)>) {
                            if (!is_nil(val)) {
                                used_refs[get_id(val)].push_back(ebm::AnyRef{item.id.id});
                            }
                        }
                        else
                            VISITOR_RECURSE_CONTAINER(visitor, name, val)
                        else VISITOR_RECURSE(visitor, name, val)
                    });
                }
            };
            map_to(ctx.statement_repository().get_all());
            map_to(ctx.identifier_repository().get_all());
            map_to(ctx.type_repository().get_all());
            map_to(ctx.string_repository().get_all());
            map_to(ctx.expression_repository().get_all());
            for (auto& alias : ctx.alias_vector()) {
                if (used_refs.find(get_id(alias.from)) == used_refs.end()) {
                    print_if_verbose("Removing unused alias: ", get_id(alias.from), "\n");
                    continue;  // Skip unused aliases
                }
                switch (alias.hint) {
                    case ebm::AliasHint::IDENTIFIER:
                        used_refs[get_id(alias.to)].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::STATEMENT:
                        used_refs[get_id(alias.to)].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::STRING:
                        used_refs[get_id(alias.to)].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::EXPRESSION:
                        used_refs[get_id(alias.to)].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::TYPE:
                        used_refs[get_id(alias.to)].push_back(ebm::AnyRef{alias.from.id});
                        break;
                    case ebm::AliasHint::ALIAS:
                        return unexpect_error("Alias hint should not contains ALIAS: {} -> {}", get_id(alias.from), get_id(alias.to));
                }
            }
            std::set<std::uint64_t> should_remove;
            for (size_t i = 2 /*0 is nil, 1 is root node*/; i < max_id.value(); i++) {
                auto found = used_refs.find(i);
                if (found != used_refs.end()) {
                    continue;
                }
                should_remove.insert(i);
            }
            // simply remove
            auto remove = [&](auto&& rem) {
                std::erase_if(rem, [&](const auto& item) {
                    if (should_remove.find(get_id(item.id)) != should_remove.end()) {
                        print_if_verbose("Removing unused item: ", get_id(item.id));
                        if constexpr (has_body_kind<decltype(item)>) {
                            print_if_verbose("(", to_string(item.body.kind), ")");
                        }
                        print_if_verbose("\n");
                        return true;
                    }
                    return false;
                });
            };
            remove(ctx.statement_repository().get_all());
            remove(ctx.identifier_repository().get_all());
            remove(ctx.type_repository().get_all());
            remove(ctx.string_repository().get_all());
            remove(ctx.expression_repository().get_all());
            std::erase_if(ctx.alias_vector(), [&](const auto& alias) {
                return should_remove.find(get_id(alias.from)) != should_remove.end() ||
                       should_remove.find(get_id(alias.to)) != should_remove.end();
            });
            return used_refs.size();
        };
        futils::test::Timer t;
        MAYBE(first_size, trial());
        MAYBE(second_size, trial());
        while (first_size != second_size) {
            first_size = second_size;
            MAYBE(third_size, trial());
            second_size = third_size;
        }
        print_if_verbose("Removed unused items in ", t.next_step<std::chrono::microseconds>(), "\n");
        std::vector<std::tuple<ebm::AnyRef, size_t>> most_used;
        for (const auto& [id, refs] : used_refs) {
            most_used.emplace_back(ebm::AnyRef{id}, refs.size());
        }
        std::stable_sort(most_used.begin(), most_used.end(), [](const auto& a, const auto& b) {
            return std::get<1>(a) > std::get<1>(b);
        });
        std::map<size_t, ebm::AnyRef> old_to_new;
        ctx.set_max_id(0);  // reset
        for (auto& mapping : most_used) {
            MAYBE(new_id, ctx.new_id());
            old_to_new[get_id(std::get<0>(mapping))] = new_id;
        }
        MAYBE(entry_id, ctx.new_id());
        old_to_new[1] = entry_id;
        auto remap = [&](auto& vec) {
            t.reset();
            for (auto& item : vec) {
                if (auto it = old_to_new.find(get_id(item.id)); it != old_to_new.end()) {
                    item.id.id = it->second.id;
                }
                item.body.visit([&](auto&& visitor, const char* name, auto&& val, std::optional<size_t> index = std::nullopt) -> void {
                    if constexpr (AnyRef<decltype(val)>) {
                        if (!is_nil(val)) {
                            auto it = old_to_new.find(get_id(val));
                            if (it != old_to_new.end()) {
                                val.id = it->second.id;
                            }
                        }
                    }
                    else if constexpr (is_container<decltype(val)>) {
                        for (size_t i = 0; i < val.container.size(); ++i) {
                            visitor(visitor, name, val.container[i], i);
                        }
                    }
                    else
                        VISITOR_RECURSE(visitor, name, val)
                });
            }
            print_if_verbose("Remap ", vec.size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
            t.reset();
            std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
                return get_id(a.id) < get_id(b.id);
            });
            print_if_verbose("Sort ", vec.size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        };
        remap(ctx.statement_repository().get_all());
        remap(ctx.identifier_repository().get_all());
        remap(ctx.type_repository().get_all());
        remap(ctx.string_repository().get_all());
        remap(ctx.expression_repository().get_all());
        t.reset();
        for (auto& alias : ctx.alias_vector()) {
            if (auto it = old_to_new.find(get_id(alias.from)); it != old_to_new.end()) {
                alias.from.id = it->second.id;
            }
            if (auto it = old_to_new.find(get_id(alias.to)); it != old_to_new.end()) {
                alias.to.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.alias_vector().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        std::sort(ctx.alias_vector().begin(), ctx.alias_vector().end(), [](const auto& a, const auto& b) {
            return get_id(a.from) < get_id(b.from);
        });
        print_if_verbose("Sort ", ctx.alias_vector().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        std::erase_if(ctx.debug_locations(), [&](auto& d) {
            return !old_to_new.contains(get_id(d.ident));
        });
        print_if_verbose("Removed unreferenced debug information in ", t.delta<std::chrono::microseconds>(), "\n");
        for (auto& loc : ctx.debug_locations()) {
            if (auto it = old_to_new.find(get_id(loc.ident)); it != old_to_new.end()) {
                loc.ident.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.debug_locations().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        return {};
    }

}  // namespace ebmgen
