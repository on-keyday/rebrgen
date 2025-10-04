/*license*/
#include <file/file_view.h>
#include <core/ast/file.h>
#include <core/ast/json.h>
#include <json/parser.h>
#include <json/constructor.h>
#include <string_view>
#include "load_json.hpp"

namespace ebmgen {
    struct DecoderObserver {
        std::uint64_t node_count = 0;
        std::uint64_t scope_count = 0;
        bool next_int_is_node_count = false;
        bool next_int_is_scope_count = false;
        bool next_array_is_node = false;
        bool next_array_is_scope = false;
        void observe(futils::json::ElementType t, auto& s) {
            if (t == futils::json::ElementType::key_string) {
                if (s.size() != 4) {
                    return;
                }
                if (s.back().get_holder().init_as_string() == "node_count") {
                    next_int_is_node_count = true;
                }
                else if (s.back().get_holder().init_as_string() == "scope_count") {
                    next_int_is_scope_count = true;
                }
                else if (s.back().get_holder().init_as_string() == "node") {
                    next_array_is_node = true;
                }
                else if (s.back().get_holder().init_as_string() == "scope") {
                    next_array_is_scope = true;
                }
            }
            else if (t == futils::json::ElementType::integer) {
                if (next_int_is_node_count) {
                    node_count = *s.back().get_holder().as_numi();
                    next_int_is_node_count = false;
                }
                else if (next_int_is_scope_count) {
                    scope_count = *s.back().get_holder().as_numi();
                    next_int_is_scope_count = false;
                }
            }
            else if (t == futils::json::ElementType::array) {
                if (next_array_is_node) {
                    next_array_is_node = false;
                    s.back().get_holder().init_as_array().reserve(node_count);
                }
                else if (next_array_is_scope) {
                    next_array_is_scope = false;
                    s.back().get_holder().init_as_array().reserve(scope_count);
                }
            }
        }
    };

    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json_file(futils::view::rvec input, std::function<void(const char*)> timer_cb) {
        if (timer_cb) timer_cb("json file open");
        futils::json::BytesLikeReader<futils::view::rvec> r{input};
        r.size = r.bytes.size();
        futils::json::JSONConstructor<futils::json::JSON, futils::json::StaticStack<15, futils::json::JSON>, DecoderObserver> jc;
        futils::json::GenericConstructor<decltype(r)&, futils::json::StaticStack<8, futils::json::ParseStateDetail>, decltype(jc)&> g{r, jc};
        futils::json::Parser<decltype(g)&> p{g};
        p.skip_space();
        auto result = p.parse();
        if (result != futils::json::ParseResult::end) {
            return unexpect_error("cannot parse json file: {} {}", futils::json::to_string(result), futils::json::to_string(p.state.state()));
        }
        if (timer_cb) timer_cb("json file parse");
        auto js = std::move(jc.stack.back());
        brgen::ast::AstFile file;
        if (!futils::json::convert_from_json(js, file)) {
            return unexpect_error("cannot convert json file");
        }
        if (!file.ast) {
            return unexpect_error("ast is not found");
        }
        if (timer_cb) timer_cb("json file convert");
        brgen::ast::JSONConverter c;
        auto res = c.decode(*file.ast);
        if (!res) {
            return unexpect_error("cannot decode json file: {}", res.error().locations[0].msg);
        }
        if (!*res) {
            return unexpect_error("cannot decode json file");
        }
        if (timer_cb) timer_cb("json file decode");
        return std::pair{*res, file.files};
    }

    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json(std::string_view input, std::function<void(const char*)> timer_cb) {
        futils::file::View view;
        if (auto res = view.open(input); !res) {
            return unexpect_error(Error(res.error()));
        }
        return load_json_file(futils::view::rvec(view), timer_cb);
    }

    expected<void> decode_json_ebm(std::string_view input) {
        futils::json::BytesLikeReader<futils::view::rvec> r{input};
        r.size = r.bytes.size();
        futils::json::JSONConstructor<futils::json::JSON, futils::json::StaticStack<15, futils::json::JSON>> jc;
        futils::json::GenericConstructor<decltype(r)&, futils::json::StaticStack<8, futils::json::ParseStateDetail>, decltype(jc)&> g{r, jc};
        futils::json::Parser<decltype(g)&> p{g};
        p.skip_space();
        auto result = p.parse();
        if (result != futils::json::ParseResult::end) {
            return unexpect_error("cannot parse json file: {} {}", futils::json::to_string(result), futils::json::to_string(p.state.state()));
        }
        auto js = std::move(jc.stack.back());
        ebm::ExtendedBinaryModule ebm;
        js.get_holder().as_str();
    }
}  // namespace ebmgen
