#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <memory>
#include <unordered_map>
#include "convert/helper.hpp"

namespace ebmgen {
    enum class GenerateType {
        Normal,
        Encode,
        Decode,
    };

    struct IdentifierSource {
       private:
        std::uint64_t next_id = 1;

       public:
        expected<ebm::Varint> new_id() {
            auto v = varint(next_id);
            if (!v) {
                return unexpect_error(std::move(v.error()));
            }
            next_id++;
            return v;
        }
    };

    template <typename T>
    expected<std::string> serialize(const T& body) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        auto err = body.encode(w);
        if (err) {
            return unexpect_error(err);
        }
        return buffer;
    }

    template <class ID, class Instance, class Body>
    struct ReferenceRepository {
        expected<ID> new_id(IdentifierSource& source) {
            return source.new_id().and_then([this](ebm::Varint id) -> expected<ID> {
                return ID{id};
            });
        }
        expected<ID> add(ID id, Body&& body) {
            identifier_map[id.id.value()] = instances.size();
            Instance instance;
            instance.id = id;
            instance.body = std::move(body);
            instances.push_back(std::move(instance));
            return id;
        }

        expected<ID> add(IdentifierSource& source, Body&& body) {
            auto serialized = serialize(body);
            if (!serialized) {
                return unexpect_error(std::move(serialized.error()));
            }
            if (auto it = cache.find(*serialized); it != cache.end()) {
                return it->second;
            }
            auto id = new_id(source);
            if (!id) {
                return unexpect_error(std::move(id.error()));
            }
            return add(*id, std::move(body));
        }

        Instance* get(const ID& id) {
            if (id.id.value() == 0 || identifier_map.find(id.id.value()) == identifier_map.end()) {
                return nullptr;
            }
            return &instances[identifier_map[id.id.value()]];
        }

        std::vector<Instance>& get_all() {
            return instances;
        }

       private:
        std::unordered_map<std::string, ID> cache;
        std::unordered_map<uint64_t, size_t> identifier_map;
        std::vector<Instance> instances;
    };
    bool is_alignment_vector(const std::shared_ptr<ast::Field>& t);

    struct FormatEncodeDecode {
        ebm::StatementRef encode;
        ebm::TypeRef encode_type;
        ebm::StatementRef decode;
        ebm::TypeRef decode_type;
    };

    struct StatementConverter;
    struct ExpressionConverter;
    struct EncoderConverter;
    struct DecoderConverter;
    struct TypeConverter;

    template <typename T>
    concept AnyRef = requires(T t) {
        { t.id } -> std::convertible_to<ebm::Varint>;
    };

    struct ConverterContext {
       private:
        IdentifierSource ident_source;
        GenerateType current_generate_type = GenerateType::Normal;
        std::unordered_map<std::shared_ptr<ast::Node>, ebm::StatementRef> visited_nodes;
        std::unordered_map<std::shared_ptr<ast::Node>, FormatEncodeDecode> format_encode_decode;

        ReferenceRepository<ebm::IdentifierRef, ebm::Identifier, ebm::String> identifier_repo;
        ReferenceRepository<ebm::StringRef, ebm::StringLiteral, ebm::String> string_repo;
        ReferenceRepository<ebm::TypeRef, ebm::Type, ebm::TypeBody> type_repo;
        ReferenceRepository<ebm::ExpressionRef, ebm::Expression, ebm::ExpressionBody> expression_repo;
        ReferenceRepository<ebm::StatementRef, ebm::Statement, ebm::StatementBody> statement_repo;

        std::vector<ebm::Loc> debug_locs;

        ebm::Endian global_endian = ebm::Endian::big;
        ebm::Endian local_endian = ebm::Endian::unspec;
        ebm::StatementRef current_dynamic_endian = ebm::StatementRef{};
        bool on_function = false;

        std::shared_ptr<StatementConverter> statement_converter;
        std::shared_ptr<ExpressionConverter> expression_converter;
        std::shared_ptr<EncoderConverter> encoder_converter;
        std::shared_ptr<DecoderConverter> decoder_converter;
        std::shared_ptr<TypeConverter> type_converter;

       public:
        ConverterContext();

        expected<void> finalize(ebm::ExtendedBinaryModule& mod);

        template <AnyRef T>
        expected<void> add_debug_loc(brgen::lexer::Loc loc, T ref) {
            ebm::Loc debug_loc;
            MAYBE(file_id, varint(loc.file));
            MAYBE(line, varint(loc.line));
            MAYBE(column, varint(loc.col));
            MAYBE(start, varint(loc.pos.begin));
            MAYBE(end, varint(loc.pos.end));
            debug_loc.ident = ebm::AnyRef{ref.id};
            debug_loc.file_id = file_id;
            debug_loc.line = line;
            debug_loc.column = column;
            debug_loc.start = start;
            debug_loc.end = end;
            debug_locs.push_back(std::move(debug_loc));
            return {};
        }

        StatementConverter& get_statement_converter();

        ExpressionConverter& get_expression_converter();

        EncoderConverter& get_encoder_converter();

        DecoderConverter& get_decoder_converter();

        TypeConverter& get_type_converter();

        expected<ebm::IdentifierRef> anonymous_identifier() {
            return identifier_repo.new_id(ident_source);
        }

        expected<ebm::StatementRef> new_statement_id() {
            return statement_repo.new_id(ident_source);
        }

        IdentifierSource& get_identifier_source() {
            return ident_source;
        }

        GenerateType get_current_generate_type() const {
            return current_generate_type;
        }

        void set_current_generate_type(GenerateType type) {
            current_generate_type = type;
        }

        expected<ebm::EndianExpr> get_endian(ebm::Endian base, bool sign);
        bool set_endian(ebm::Endian e, ebm::StatementRef id = ebm::StatementRef{});

        bool is_on_function() const {
            return on_function;
        }

        void set_on_function(bool value) {
            on_function = value;
        }

        void add_visited_node(const std::shared_ptr<ast::Node>& node, ebm::StatementRef ref) {
            visited_nodes[node] = ref;
        }

        std::optional<ebm::StatementRef> is_visited(const std::shared_ptr<ast::Node>& node) const {
            auto it = visited_nodes.find(node);
            if (it != visited_nodes.end()) {
                return it->second;
            }
            return std::nullopt;
        }

        void add_format_encode_decode(const std::shared_ptr<ast::Node>& node,
                                      ebm::StatementRef encode,
                                      ebm::TypeRef encode_type,
                                      ebm::StatementRef decode,
                                      ebm::TypeRef decode_type) {
            format_encode_decode[node] = FormatEncodeDecode{
                encode,
                encode_type,
                decode,
                decode_type,
            };
        }

        expected<FormatEncodeDecode> get_format_encode_decode(const std::shared_ptr<ast::Node>& node) {
            auto it = format_encode_decode.find(node);
            if (it != format_encode_decode.end()) {
                return it->second;
            }
            return unexpect_error("Format encode/decode not found for node");
        }

        expected<ebm::IdentifierRef> add_identifier(const std::string& name) {
            auto len = varint(name.size());
            if (!len) {
                return unexpect_error("Failed to create varint for identifier length: {}", len.error().error());
            }
            ebm::String string_literal;
            string_literal.data = name;
            string_literal.length = *len;
            return identifier_repo.add(ident_source, std::move(string_literal));
        }

        expected<ebm::TypeRef> add_type(ebm::TypeBody&& body) {
            return type_repo.add(ident_source, std::move(body));
        }

        expected<ebm::StatementRef> add_statement(ebm::StatementRef id, ebm::StatementBody&& body) {
            return statement_repo.add(id, std::move(body));
        }

        expected<ebm::StatementRef> add_statement(ebm::StatementBody&& body) {
            return statement_repo.add(ident_source, std::move(body));
        }

        expected<ebm::StringRef> add_string(const std::string& str) {
            auto len = varint(str.size());
            if (!len) {
                return unexpect_error("Failed to create varint for string length: {}", len.error().error());
            }
            ebm::String string_literal;
            string_literal.data = str;
            string_literal.length = *len;
            return string_repo.add(ident_source, std::move(string_literal));
        }

        expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body) {
            return expression_repo.add(ident_source, std::move(body));
        }

        expected<ebm::TypeRef> get_counter_type();
        expected<ebm::TypeRef> get_unsigned_n_int(size_t n);
        expected<ebm::TypeRef> get_u8_n_array(size_t n);
        expected<ebm::TypeRef> get_bool_type();
        expected<ebm::TypeRef> get_void_type();
        expected<ebm::TypeRef> get_encoder_return_type();
        expected<ebm::TypeRef> get_decoder_return_type();

        expected<ebm::ExpressionRef> get_int_literal(std::uint64_t value);

        // shorthand for creating a type with a single kind
        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);

        ebm::Statement* get_statement(const ebm::StatementRef& ref) {
            return statement_repo.get(ref);
        }

        ebm::Type* get_type(const ebm::TypeRef& ref) {
            return type_repo.get(ref);
        }
    };

    expected<std::string> decode_base64(const std::shared_ptr<ast::StrLiteral>& lit);

    /*
        class Converter {
           public:
            Converter(ebm::ExtendedBinaryModule& ebm)
                : ebm(ebm) {}

            expected<void> convert(const std::shared_ptr<brgen::ast::Node>& ast_root);

           private:
            friend struct ConverterProxy;
            ebm::ExtendedBinaryModule& ebm;
            std::shared_ptr<ast::Node> root;
            Error err;
            // std::uint64_t next_id = 1;
            IdentifierSource ident_source;
            GenerateType current_generate_type = GenerateType::Normal;
            std::unordered_map<std::shared_ptr<ast::Node>, ebm::StatementRef> visited_nodes;

            ReferenceRepository<ebm::IdentifierRef, ebm::Identifier, ebm::String> identifier_repo;
            ReferenceRepository<ebm::StringRef, ebm::StringLiteral, ebm::String> string_repo;
            ReferenceRepository<ebm::TypeRef, ebm::Type, ebm::TypeBody> type_repo;
            ReferenceRepository<ebm::ExpressionRef, ebm::Expression, ebm::ExpressionBody> expression_repo;
            ReferenceRepository<ebm::StatementRef, ebm::Statement, ebm::StatementBody> statement_repo;

            std::unordered_map<std::shared_ptr<ast::Node>, FormatEncodeDecode> format_encode_decode;
            void add_format_encode_decode(const std::shared_ptr<ast::Node>& node,
                                          ebm::StatementRef encode,
                                          ebm::TypeRef encode_type,
                                          ebm::StatementRef decode,
                                          ebm::TypeRef decode_type) {
                format_encode_decode[node] = FormatEncodeDecode{
                    encode,
                    encode_type,
                    decode,
                    decode_type,
                };
            }

            expected<FormatEncodeDecode> get_format_encode_decode(const std::shared_ptr<ast::Node>& node) {
                auto it = format_encode_decode.find(node);
                if (it != format_encode_decode.end()) {
                    return it->second;
                }
                return unexpect_error("Format encode/decode not found for node");
            }

            expected<ebm::IdentifierRef> add_identifier(const std::string& name) {
                auto len = varint(name.size());
                if (!len) {
                    return unexpect_error("Failed to create varint for identifier length: {}", len.error().error());
                }
                ebm::String string_literal;
                string_literal.data = name;
                string_literal.length = *len;
                return identifier_repo.add(ident_source, std::move(string_literal));
            }

            expected<ebm::TypeRef> add_type(ebm::TypeBody&& body) {
                return type_repo.add(ident_source, std::move(body));
            }

            expected<ebm::StatementRef> add_statement(ebm::StatementRef id, ebm::StatementBody&& body) {
                return statement_repo.add(id, std::move(body));
            }

            expected<ebm::StatementRef> add_statement(ebm::StatementBody&& body) {
                return statement_repo.add(ident_source, std::move(body));
            }

            expected<ebm::StringRef> add_string(const std::string& str) {
                auto len = varint(str.size());
                if (!len) {
                    return unexpect_error("Failed to create varint for string length: {}", len.error().error());
                }
                ebm::String string_literal;
                string_literal.data = str;
                string_literal.length = *len;
                return string_repo.add(ident_source, std::move(string_literal));
            }

            expected<ebm::ExpressionRef> add_expr(ebm::ExpressionBody&& body) {
                return expression_repo.add(ident_source, std::move(body));
            }

            expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);
            expected<ebm::CastType> get_cast_type(ebm::TypeRef dest, ebm::TypeRef src);

            expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
            expected<ebm::StatementRef> convert_statement_impl(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node);
            expected<ebm::StatementRef> encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
            expected<void> encode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
            expected<void> encode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
            expected<void> encode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
            expected<void> encode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
            expected<void> encode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
            expected<void> encode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
            expected<ebm::StatementRef> decode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
            expected<ebm::ExpressionRef> get_alignment_requirement(std::uint64_t alignment_bytes, ebm::StreamType type);
            expected<void> construct_string_array(ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate);

            expected<ebm::ExpressionRef> get_max_value_expr(ebm::TypeRef type);

            expected<void> set_lengths();

            ebm::Endian global_endian = ebm::Endian::big;
            ebm::Endian local_endian = ebm::Endian::unspec;
            ebm::StatementRef current_dynamic_endian = ebm::StatementRef{};
            bool on_function = false;

            expected<ebm::EndianExpr> get_endian(ebm::Endian base, bool sign);
            bool set_endian(ebm::Endian e, ebm::StatementRef id = ebm::StatementRef{});
            expected<ebm::TypeRef> get_counter_type();

            expected<ebm::TypeRef> get_unsigned_n_int(size_t n);
            expected<ebm::TypeRef> get_u8_n_array(size_t n);
            expected<ebm::TypeRef> get_bool_type();
            expected<ebm::TypeRef> get_void_type();
            expected<ebm::TypeRef> get_encoder_return_type();
            expected<ebm::TypeRef> get_decoder_return_type();

            expected<ebm::StatementRef> encode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef from, ebm::TypeRef cast_from);
            expected<ebm::StatementRef> decode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef to, ebm::TypeRef cast_to);

            expected<ebm::ExpressionRef> get_int_literal(std::uint64_t value);

            expected<ebm::StatementRef> add_endian_specific(ebm::EndianExpr endian, auto&& on_little_endian, auto&& on_big_endian) {
                ebm::StatementRef ref;
                const auto is_native_or_dynamic = endian.endian() == ebm::Endian::native || endian.endian() == ebm::Endian::dynamic;
                if (is_native_or_dynamic) {
                    ebm::ExpressionBody is_little;
                    is_little.op = ebm::ExpressionOp::IS_LITTLE_ENDIAN;
                    is_little.endian_expr(endian.dynamic_ref);  // if native, this will be empty
                    auto is_little_ref = add_expr(std::move(is_little));
                    if (!is_little_ref) {
                        return unexpect_error(std::move(is_little_ref.error()));
                    }
                    expected<ebm::StatementRef> then_block = on_little_endian();
                    if (!then_block) {
                        return unexpect_error(std::move(then_block.error()));
                    }
                    expected<ebm::StatementRef> else_block = on_big_endian();
                    if (!else_block) {
                        return unexpect_error(std::move(else_block.error()));
                    }
                    auto res = add_statement(make_if_statement(*is_little_ref, *then_block, *else_block));
                    if (!res) {
                        return unexpect_error(std::move(res.error()));
                    }
                    ref = *res;
                }
                else if (endian.endian() == ebm::Endian::little) {
                    expected<ebm::StatementRef> res = on_little_endian();
                    if (!res) {
                        return unexpect_error(std::move(res.error()));
                    }
                    ref = *res;
                }
                else if (endian.endian() == ebm::Endian::big) {
                    expected<ebm::StatementRef> res = on_big_endian();
                    if (!res) {
                        return unexpect_error(std::move(res.error()));
                    }
                    ref = *res;
                }
                else {
                    return unexpect_error("Unsupported endian type: {}", to_string(endian.endian()));
                }
                return ref;
            }
        };
    */
    struct StatementConverter {
        ConverterContext& ctx;
        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);

       private:
        expected<ebm::StatementRef> convert_statement_impl(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node);

        expected<ebm::StatementBody> convert_loop_body(const std::shared_ptr<ast::Loop>& node);
    };

    struct ExpressionConverter {
        ConverterContext& ctx;
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);

       private:
        expected<void> convert_expr_impl(const std::shared_ptr<ast::IntLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::BoolLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::StrLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::TypeLiteral>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Ident>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Binary>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Unary>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Index>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::MemberAccess>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Cast>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Range>& node, ebm::ExpressionBody& body);
        expected<void> convert_expr_impl(const std::shared_ptr<ast::IOOperation>& node, ebm::ExpressionBody& body);
        // Fallback for unhandled types
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Expr>& node, ebm::ExpressionBody& body);
    };

    expected<ebm::ExpressionRef> get_alignment_requirement(ConverterContext& ctx, std::uint64_t alignment_bytes, ebm::StreamType type);

    struct EncoderConverter {
        ConverterContext& ctx;
        expected<ebm::StatementRef> encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);

       private:
        expected<void> encode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> encode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> encode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<void> encode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<void> encode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> encode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);

        expected<ebm::StatementRef> encode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef from, ebm::TypeRef cast_from);
    };

    struct DecoderConverter {
        ConverterContext& ctx;
        expected<ebm::StatementRef> decode(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);
        expected<ebm::StatementRef> decode_multi_byte_int_with_fixed_array(size_t n, ebm::EndianExpr endian, ebm::ExpressionRef to, ebm::TypeRef cast_to);
    };

    struct TypeConverter {
        ConverterContext& ctx;
        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);
        expected<ebm::CastType> get_cast_type(ebm::TypeRef dest, ebm::TypeRef src);
    };

    expected<ebm::StatementRef> add_endian_specific(ConverterContext& ctx, ebm::EndianExpr endian, std::function<expected<ebm::StatementRef>()> on_little_endian, std::function<expected<ebm::StatementRef>()> on_big_endian);
    expected<void> construct_string_array(ConverterContext& ctx, ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate);

    expected<ebm::StatementBody> assert_statement_body(ConverterContext& ctx, ebm::ExpressionRef condition);
    expected<ebm::StatementRef> assert_statement(ConverterContext& ctx, ebm::ExpressionRef condition);
}  // namespace ebmgen
