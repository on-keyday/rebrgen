/*license*/
#pragma once
#include <string>
#include <string_view>
#include <map>

namespace rebgn {
    std::string env_escape(std::string_view str, std::map<std::string, std::string>& map);
    std::string unimplemented_comment(Flags& flags, const std::string& op);
    enum class EvalResultMode {
        TEXT,
        PASSTHROUGH,
        COMMENT,  // treat as TEXT but special handling for comment
    };
    void do_make_eval_result(bm2::TmpCodeWriter& w, AbstractOp op, Flags& flags, const std::string& text, EvalResultMode mode);
    void write_eval_result(bm2::TmpCodeWriter& eval_result, Flags& flags);
    void write_eval_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& eval, Flags& flags);
    void write_inner_function_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_function, Flags& flags);
    void write_field_accessor_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& field_accessor,
                                   bm2::TmpCodeWriter& type_accessor, Flags& flags);
    void write_inner_block_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_block, Flags& flags);
    void write_parameter_func(bm2::TmpCodeWriter& w,
                              bm2::TmpCodeWriter& add_parameter,
                              bm2::TmpCodeWriter& add_call_parameter, Flags& flags);
    void write_type_to_string_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& type_to_string, Flags& flags);
}  // namespace rebgn
