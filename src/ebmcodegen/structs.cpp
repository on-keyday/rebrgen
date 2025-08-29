/*license*/
#include "stub/structs.hpp"
#include "../ebmgen/common.hpp"
namespace ebmcodegen {
    std::map<std::string_view, Struct> make_struct_map() {
        std::vector<Struct> structs;
        structs.push_back({
            ebm::ExtendedBinaryModule::visitor_name,
        });
        std::map<std::string_view, Struct> struct_map;

        ebm::ExtendedBinaryModule::visit_static([&](auto&& visitor, const char* name, auto tag, TypeAttribute dispatch = NONE) -> void {
            using T = typename decltype(tag)::type;
            if constexpr (ebmgen::has_visit<T, decltype(visitor)>) {
                structs.back().fields.push_back({
                    name,
                    T::visitor_name,
                    dispatch,
                });
                if constexpr (!ebmgen::AnyRef<T>) {
                    structs.push_back({
                        T::visitor_name,
                    });
                    T::visit_static(visitor);
                    auto s = std::move(structs.back());
                    structs.pop_back();
                    struct_map[s.name] = std::move(s);
                }
            }
            else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
                using P = typename futils::helper::template_instance_of_t<T, std::vector>::template param_at<0>;
                visitor(visitor, name, ebm::ExtendedBinaryModule::visitor_tag<P>{}, TypeAttribute(dispatch | TypeAttribute::ARRAY));
            }
            else if constexpr (std::is_pointer_v<T>) {
                using P = std::remove_pointer_t<T>;
                visitor(visitor, name, ebm::ExtendedBinaryModule::visitor_tag<P>{}, TypeAttribute(dispatch | TypeAttribute::PTR));
            }
            else if constexpr (std::is_enum_v<T>) {
                constexpr const char* enum_name = visit_enum(T{});
                structs.back().fields.push_back({
                    name,
                    enum_name,
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::uint64_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::uint64_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::uint8_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::uint8_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::int32_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::int32_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::int8_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::int8_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, float>) {
                structs.back().fields.push_back({
                    name,
                    "float",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, double>) {
                structs.back().fields.push_back({
                    name,
                    "double",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, bool>) {
                structs.back().fields.push_back({
                    name,
                    "bool",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                structs.back().fields.push_back({
                    name,
                    "std::string",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, const char(&)[5]>) {  // skip
            }
            else {
                static_assert(std::is_same_v<T, void>, "Unsupported type");
            }
        });
        struct_map["ExtendedBinaryModule"] = std::move(structs[0]);
        return struct_map;
    }
}  // namespace ebmcodegen
