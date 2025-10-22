/*license*/
#pragma once
#include <comb2/composite/range.h>
#include <comb2/basic/logic.h>
#include <comb2/basic/peek.h>
#include <comb2/basic/literal.h>
#include <comb2/basic/group.h>
#include "comb2/internal/test.h"
#include "comb2/lexctx.h"
#include "comb2/seqrange.h"
#include "ebmgen/common.hpp"
#include <vector>
#include <string_view>

namespace ebmcodegen::dsl {
    namespace syntax {
        using namespace futils::comb2::ops;
        namespace cps = futils::comb2::composite;
        enum class OutputKind {
            CppLiteral,
            CppWrite,
            TargetLang,
            Indent,
            Dedent,
            Line,
        };
        constexpr auto begin_cpp = lit("{%");
        constexpr auto end_cpp = lit("%}");
        constexpr auto begin_cpp_var_writer = lit("{{");
        constexpr auto end_cpp_var_writer = lit("}}");
        constexpr auto cpp_code = [](OutputKind tag, auto end) { return str(tag, *(not_(end) & +uany)); };
        constexpr auto cpp_target = begin_cpp & cpp_code(OutputKind::CppLiteral, end_cpp) & end_cpp;
        constexpr auto cpp_var_writer = begin_cpp_var_writer & cpp_code(OutputKind::CppWrite, end_cpp_var_writer) & end_cpp_var_writer;
        constexpr auto indent = str(OutputKind::Indent, bol & ~cps::space);
        constexpr auto line = str(OutputKind::Line, cps::eol);
        constexpr auto target_lang = str(OutputKind::TargetLang, ~(not_(cps::eol | begin_cpp | begin_cpp_var_writer) & uany));

        constexpr auto dsl = *(cpp_target | cpp_var_writer | indent | line | target_lang) & eos;

        constexpr auto test_syntax() {
            auto seq = futils::make_ref_seq(R"(
{% int a = 0; %}
if ({{ a }} > 0) {
    {{ a }} += 1;
})");
            return dsl(seq, futils::comb2::test::TestContext<OutputKind>{}, 0) == futils::comb2::Status::match;
        }

        static_assert(test_syntax(), "DSL syntax static assert");
    }  // namespace syntax

    struct DSLNode {
        syntax::OutputKind kind{};
        std::string_view content;
    };

    struct DSLContext : futils::comb2::LexContext<syntax::OutputKind> {
        std::vector<DSLNode> nodes;

        constexpr void end_string(futils::comb2::Status res, syntax::OutputKind kind, auto&& seq, futils::comb2::Pos pos) {
            if (res == futils::comb2::Status::match) {
                DSLNode node;
                node.kind = kind;
                futils::comb2::seq_range_to_string(node.content, seq, pos);
                nodes.push_back(std::move(node));
            }
        }
    };

    ebmgen::expected<std::string> generate_dsl_output(std::string_view source);

}  // namespace ebmcodegen::dsl
