/*license*/
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/converter.hpp"
#include "ebmgen/mapping.hpp"
#include "transform.hpp"
#include <cstddef>
#include <unordered_set>
#include <testutil/timer.h>

namespace ebmgen {
    template <class B>
    concept has_body_kind = requires(B b) {
        { b.body.kind };
    };

    using InverseRefs = std::unordered_map<size_t, std::vector<ebm::AnyRef>>;

    expected<InverseRefs> mark_and_sweep(TransformContext& ctx, std::function<void(const char*)> timer) {
        MappingTable table{
            EBMProxy(ctx.statement_repository().get_all(),
                     ctx.expression_repository().get_all(),
                     ctx.type_repository().get_all(),
                     ctx.identifier_repository().get_all(),
                     ctx.string_repository().get_all(),
                     ctx.alias_vector(),
                     ctx.debug_locations())};
        std::unordered_set<size_t> reachable;
        InverseRefs inverse_refs;  // ref -> item.id
        auto search = [&](auto&& search, const auto& item) -> void {
            item.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                if constexpr (AnyRef<decltype(val)>) {
                    if (!is_nil(val)) {
                        // if newly reachable, mark and search further
                        if (reachable.insert(get_id(val)).second) {
                            auto object = table.get_object(val);
                            std::visit(
                                [&](auto&& obj) -> void {
                                    using T = std::decay_t<decltype(obj)>;
                                    if constexpr (std::is_pointer_v<T>) {
                                        search(search, *obj);
                                    }
                                },
                                object);
                        }
                        inverse_refs[get_id(val)].push_back(to_any_ref(item.id));
                    }
                }
                else
                    VISITOR_RECURSE_CONTAINER(visitor, name, val)
                else VISITOR_RECURSE(visitor, name, val)
            });
        };
        // root is item_id == 1
        reachable.insert(1);
        {
            ebm::StatementRef root{1};
            auto object = table.get_object(root);
            std::visit(
                [&](auto&& obj) -> void {
                    using T = std::decay_t<decltype(obj)>;
                    if constexpr (std::is_pointer_v<T>) {
                        search(search, *obj);
                    }
                },
                object);
        }
        // also, reachable from file names
        for (const auto& d : ctx.file_names()) {
            reachable.insert(get_id(d));
            inverse_refs[get_id(d)].push_back(ebm::AnyRef{1});  // used from root
        }
        if (timer) {
            timer("mark phase");
        }
        // sweep
        size_t remove_count = 0;
        auto remove = [&](auto& rem) {
            std::decay_t<decltype(rem)> new_vec;
            new_vec.reserve(rem.size());
            for (auto& r : rem) {
                if (reachable.find(get_id(r.id)) == reachable.end()) {
                    remove_count++;
                    if (ebmgen::verbose_error) {
                        print_if_verbose("Removing unused item: ", get_id(r.id));
                        if constexpr (has_body_kind<decltype(r)>) {
                            print_if_verbose("(", to_string(r.body.kind), ")");
                        }
                        print_if_verbose("\n");
                    }
                    continue;
                }
                new_vec.push_back(std::move(r));
            }
            rem = std::move(new_vec);
        };
        remove(ctx.statement_repository().get_all());
        remove(ctx.identifier_repository().get_all());
        remove(ctx.type_repository().get_all());
        remove(ctx.string_repository().get_all());
        remove(ctx.expression_repository().get_all());
        std::erase_if(ctx.alias_vector(), [&](const auto& alias) {
            return reachable.find(get_id(alias.from)) == reachable.end() ||
                   reachable.find(get_id(alias.to)) == reachable.end();
        });
        print_if_verbose("Total removed unused items: ", remove_count, "\n");
        if (timer) {
            timer("sweep phase");
        }
        return inverse_refs;
    }

    expected<void> remove_unused_object(TransformContext& ctx, std::function<void(const char*)> timer) {
        MAYBE(max_id, ctx.max_id());
        futils::test::Timer t;
        /**
        std::unordered_map<size_t, std::vector<ebm::AnyRef>> inverse_refs;  // ref -> item.id
        std::unordered_map<size_t, std::vector<ebm::AnyRef>> use_refs;      // item.id -> ref
        size_t trial_counter = 0;
        auto trial = [&] -> expected<size_t> {
            print_if_verbose("Remove Trial: ", trial_counter, "\n");
            trial_counter++;
            inverse_refs.clear();
            use_refs.clear();
            auto map_to = [&](const auto& vec) {
                for (const auto& item : vec) {
                    item.body.visit([&](auto&& visitor, const char* name, auto&& val) -> void {
                        if constexpr (AnyRef<decltype(val)>) {
                            if (!is_nil(val)) {
                                inverse_refs[get_id(val)].push_back(to_any_ref(item.id));
                                use_refs[get_id(item.id)].push_back(to_any_ref(val));
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
                if (inverse_refs.find(get_id(alias.from)) == inverse_refs.end()) {
                    print_if_verbose("Removing unused alias: ", get_id(alias.from), "\n");
                    continue;  // Skip unused aliases
                }
                switch (alias.hint) {
                    case ebm::AliasHint::IDENTIFIER:
                        inverse_refs[get_id(alias.to)].push_back(to_any_ref(alias.from));
                        break;
                    case ebm::AliasHint::STATEMENT:
                        inverse_refs[get_id(alias.to)].push_back(to_any_ref(alias.from));
                        break;
                    case ebm::AliasHint::STRING:
                        inverse_refs[get_id(alias.to)].push_back(to_any_ref(alias.from));
                        break;
                    case ebm::AliasHint::EXPRESSION:
                        inverse_refs[get_id(alias.to)].push_back(to_any_ref(alias.from));
                        break;
                    case ebm::AliasHint::TYPE:
                        inverse_refs[get_id(alias.to)].push_back(to_any_ref(alias.from));
                        break;
                    case ebm::AliasHint::ALIAS:
                        return unexpect_error("Alias hint should not contains ALIAS: {} -> {}", get_id(alias.from), get_id(alias.to));
                }
            }
            std::unordered_set<std::uint64_t> should_remove;
            for (size_t i = 2 *0 is nil, 1 is root node*; i < max_id.value(); i++) {
                auto found = inverse_refs.find(i);
                if (found != inverse_refs.end()) {
                    continue;
                }
                should_remove.insert(i);
            }
            // don't remove referenced by file names
            for (auto& d : ctx.file_names()) {
                should_remove.erase(get_id(d));
                inverse_refs[get_id(d)].push_back(ebm::AnyRef{1});  // used from root
            }
            // simply remove
            auto remove = [&](auto&& rem) {
                std::erase_if(rem, [&](const auto& item) {
                    if (should_remove.find(get_id(item.id)) != should_remove.end()) {
                        if (ebmgen::verbose_error) {
                            print_if_verbose("Removing unused item: ", get_id(item.id));
                            if constexpr (has_body_kind<decltype(item)>) {
                                print_if_verbose("(", to_string(item.body.kind), ")");
                            }
                            print_if_verbose("\n");
                            if (auto found = use_refs.find(get_id(item.id)); found != use_refs.end()) {
                                for (auto& remove_candidate : found->second) {
                                    print_if_verbose("    Next candidate: ", get_id(remove_candidate), "\n");
                                }
                            }
                        }
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
            return inverse_refs.size();
        };

        MAYBE(first_size, trial());
        MAYBE(second_size, trial());
        while (first_size != second_size) {
            first_size = second_size;
            MAYBE(third_size, trial());
            second_size = third_size;
        }
        */
        MAYBE(inverse_refs, mark_and_sweep(ctx, timer));

        print_if_verbose("Removed unused items in ", t.next_step<std::chrono::microseconds>(), "\n");
        std::vector<std::tuple<ebm::AnyRef, size_t>> most_used;
        for (const auto& [id, refs] : inverse_refs) {
            most_used.emplace_back(ebm::AnyRef{id}, refs.size());
        }
        std::stable_sort(most_used.begin(), most_used.end(), [](const auto& a, const auto& b) {
            return std::get<1>(a) > std::get<1>(b);
        });
        std::unordered_map<size_t, ebm::AnyRef> old_to_new;
        ctx.set_max_id(0);  // reset
        for (auto& mapping : most_used) {
            MAYBE(new_id, ctx.new_id());
            old_to_new[get_id(std::get<0>(mapping))] = new_id;
        }
        MAYBE(entry_id, ctx.new_id());
        old_to_new[1] = entry_id;
        auto remap = [&](auto& vec) {
            t.reset();
            std::vector<std::pair<ebm::AnyRef, size_t>> id_sort;
            size_t index = 0;
            for (auto& item : vec) {
                if (auto it = old_to_new.find(get_id(item.id)); it != old_to_new.end()) {
                    item.id.id = it->second.id;
                }
                id_sort.push_back({to_any_ref(item.id), index});
                index++;
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
            std::decay_t<decltype(vec)> new_vec;
            new_vec.resize(vec.size());
            std::sort(id_sort.begin(), id_sort.end(), [](const auto& a, const auto& b) {
                return get_id(a.first) < get_id(b.first);
            });
            size_t i = 0;
            for (auto& v : id_sort) {
                new_vec[i++] = std::move(vec[v.second]);
            }
            vec = std::move(new_vec);
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
        size_t removed_debug = 0;
        std::erase_if(ctx.debug_locations(), [&](auto& d) {
            if (!old_to_new.contains(get_id(d.ident))) {
                removed_debug++;
                return true;
            }
            return false;
        });
        print_if_verbose("Removed ", removed_debug, " unreferenced debug information in ", t.delta<std::chrono::microseconds>(), "\n");
        for (auto& loc : ctx.debug_locations()) {
            if (auto it = old_to_new.find(get_id(loc.ident)); it != old_to_new.end()) {
                loc.ident.id = it->second.id;
            }
        }
        print_if_verbose("Remap ", ctx.debug_locations().size(), " items in ", t.delta<std::chrono::microseconds>(), "\n");
        t.reset();
        for (auto& f : ctx.file_names()) {
            if (auto it = old_to_new.find(get_id(f)); it != old_to_new.end()) {
                f.id = it->second.id;
            }
        }
        if (timer) {
            timer("remap ids");
        }
        return {};
    }

}  // namespace ebmgen
