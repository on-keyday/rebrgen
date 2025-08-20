#pragma once

#include "common.hpp"
#include <core/ast/ast.h>
#include <ebm/extended_binary_module.hpp>
#include <memory>
#include <unordered_map>
#include "convert/helper.hpp"
#include "core/ast/node/ast_enum.h"
#include "core/ast/node/base.h"
#include "core/ast/node/expr.h"
#include "core/ast/node/type.h"
#include <wrap/cout.h>

namespace ebmgen {
    using GenerateType = ebm::GenerateType;
    struct VisitedKey {
        std::shared_ptr<ast::Node> node;
        GenerateType type;

        friend constexpr auto operator<=>(const VisitedKey& a, const VisitedKey& b) noexcept = default;
    };
}  // namespace ebmgen

namespace std {
    template <>
    struct hash<ebmgen::VisitedKey> {
        size_t operator()(const ebmgen::VisitedKey& key) const {
            return std::hash<std::shared_ptr<brgen::ast::Node>>{}(key.node) ^ std::hash<int>{}(static_cast<int>(key.type));
        }
    };
}  // namespace std

namespace ebmgen {

    struct ReferenceSource {
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

        expected<ebm::Varint> current_id() const {
            if (next_id == 1) {
                return null_varint;
            }
            return varint(next_id - 1);
        }

        void set_current_id(std::uint64_t id) {
            next_id = id + 1;
        }
    };

    template <typename T>
    expected<std::string> serialize(const T& body) {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        Error err = body.encode(w);
        if (err) {
            return unexpect_error(std::move(err));
        }
        return buffer;
    }

    template <AnyRef ID, class Instance, class Body, ebm::AliasHint hint>
    struct ReferenceRepository {
        ReferenceRepository(std::vector<ebm::RefAlias>& aliases)
            : aliases(aliases) {}

        expected<ID> new_id(ReferenceSource& source) {
            return source.new_id().and_then([this](ebm::Varint id) -> expected<ID> {
                return ID{id};
            });
        }

       private:
        expected<ID> add_internal(ID id, Body&& body) {
            id_index_map[id.id.value()] = instances.size();
            Instance instance;
            instance.id = id;
            instance.body = std::move(body);
            instances.push_back(std::move(instance));
            return id;
        }

       public:
        expected<ID> add(ID id, Body&& body) {
            auto serialized = serialize(body);
            if (!serialized) {
                return unexpect_error(std::move(serialized.error()));
            }
            if (auto it = cache.find(*serialized); it != cache.end()) {
                // add alias if the same body is already present
                aliases.push_back(ebm::RefAlias{
                    .hint = hint,
                    .from = ebm::AnyRef{id.id},
                    .to = ebm::AnyRef{it->second.id},
                });
                alias_id_map[id.id.value()] = it->second.id.value();
                return id;
            }
            cache[*serialized] = id;
            return add_internal(id, std::move(body));
        }

        expected<ID> add(ReferenceSource& source, Body&& body) {
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
            cache[*serialized] = id.value();
            return add_internal(*id, std::move(body));
        }

        Instance* get(const ID& id) {
            auto ref = id.id.value();
            if (auto found = alias_id_map.find(ref); found != alias_id_map.end()) {
                ref = found->second;
            }
            auto it = id_index_map.find(ref);
            if (it == id_index_map.end()) {
                return nullptr;
            }
            if (it->second >= instances.size()) {
                return nullptr;  // for safety
            }
            return &instances[it->second];
        }

        std::vector<Instance>& get_all() {
            return instances;
        }

        void clear() {
            cache.clear();
            id_index_map.clear();
            instances.clear();
            alias_id_map.clear();
        }

        void recalculate_id_index_map() {
            id_index_map.clear();
            for (size_t i = 0; i < instances.size(); ++i) {
                id_index_map[instances[i].id.id.value()] = i;
            }
        }

       private:
        std::unordered_map<std::string, ID> cache;
        std::unordered_map<uint64_t, size_t> id_index_map;
        std::vector<Instance> instances;
        std::vector<ebm::RefAlias>& aliases;  // for aliasing references
        std::unordered_map<uint64_t, uint64_t> alias_id_map;
    };
    bool is_alignment_vector(const std::shared_ptr<ast::Field>& t);

    struct FormatEncodeDecode {
        ebm::ExpressionRef encode;
        ebm::TypeRef encode_type;
        ebm::ExpressionRef decode;
        ebm::TypeRef decode_type;
    };

    struct StatementConverter;
    struct ExpressionConverter;
    struct EncoderConverter;
    struct DecoderConverter;
    struct TypeConverter;

    struct ConverterState {
       private:
        ebm::Endian global_endian = ebm::Endian::big;
        ebm::Endian local_endian = ebm::Endian::unspec;
        ebm::StatementRef current_dynamic_endian = ebm::StatementRef{};
        bool on_function = false;
        GenerateType current_generate_type = GenerateType::Normal;
        std::unordered_map<VisitedKey, ebm::StatementRef> visited_nodes;
        std::unordered_map<std::shared_ptr<ast::Node>, FormatEncodeDecode> format_encode_decode;
        ebm::Block* current_block = nullptr;

        void debug_visited(const char* action, const std::shared_ptr<ast::Node>& node, ebm::StatementRef ref, GenerateType typ) const {
            return;
            auto member = ast::as<ast::Member>(node);
            const char* ident = member && member->ident ? member->ident->ident.c_str() : "(no ident)";
            futils::wrap::cout_wrap() << action << ": (" << (node ? node_type_to_string(node->node_type) : "(null)") << " " << ident << "(" << node.get() << "), " << to_string(typ) << ")";
            if (ref.id.value() != 0) {
                futils::wrap::cout_wrap() << " -> " << ref.id.value();
            }
            futils::wrap::cout_wrap() << '\n';
        }

       public:
        [[nodiscard]] auto set_current_block(ebm::Block* block) {
            auto old = current_block;
            current_block = block;
            return futils::helper::defer([this, old]() {
                current_block = old;
            });
        }

        GenerateType get_current_generate_type() const {
            return current_generate_type;
        }

        [[nodiscard]] auto set_current_generate_type(GenerateType type) {
            auto old = current_generate_type;
            current_generate_type = type;
            return futils::helper::defer([this, old]() {
                current_generate_type = old;
            });
        }

        expected<ebm::IOAttribute> get_io_attribute(ebm::Endian base, bool sign);
        bool set_endian(ebm::Endian e, ebm::StatementRef id = ebm::StatementRef{});

        bool is_on_function() const {
            return on_function;
        }

        void set_on_function(bool value) {
            on_function = value;
        }

        void add_visited_node(const std::shared_ptr<ast::Node>& node, ebm::StatementRef ref) {
            visited_nodes[{node, current_generate_type}] = ref;
            debug_visited("Add", node, ref, current_generate_type);
        }

        expected<ebm::StatementRef> is_visited(const std::shared_ptr<ast::Node>& node, std::optional<GenerateType> t = std::nullopt) const {
            if (!t.has_value()) {
                t = current_generate_type;
            }
            auto it = visited_nodes.find({node, *t});
            if (it != visited_nodes.end()) {
                debug_visited("Found", node, it->second, *t);
                return it->second;
            }
            debug_visited("Not found", node, ebm::StatementRef{}, *t);
            auto ident = ast::as<ast::Member>(node);
            return unexpect_error("Node not visited: {} {}", !node ? "(null)" : node_type_to_string(node->node_type), ident && ident->ident ? ident->ident->ident : "(no ident)");
        }

        void add_format_encode_decode(const std::shared_ptr<ast::Node>& node,
                                      ebm::ExpressionRef encode,
                                      ebm::TypeRef encode_type,
                                      ebm::ExpressionRef decode,
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
    };

    struct EBMRepository {
       private:
        ReferenceSource ident_source;
        std::vector<ebm::RefAlias> aliases;
        ReferenceRepository<ebm::IdentifierRef, ebm::Identifier, ebm::String, ebm::AliasHint::IDENTIFIER> identifier_repo{aliases};
        ReferenceRepository<ebm::StringRef, ebm::StringLiteral, ebm::String, ebm::AliasHint::STRING> string_repo{aliases};
        ReferenceRepository<ebm::TypeRef, ebm::Type, ebm::TypeBody, ebm::AliasHint::TYPE> type_repo{aliases};
        ReferenceRepository<ebm::ExpressionRef, ebm::Expression, ebm::ExpressionBody, ebm::AliasHint::EXPRESSION> expression_repo{aliases};
        ReferenceRepository<ebm::StatementRef, ebm::Statement, ebm::StatementBody, ebm::AliasHint::STATEMENT> statement_repo{aliases};
        std::vector<ebm::Loc> debug_locs;

        friend struct TransformContext;

       public:
        EBMRepository() = default;
        EBMRepository(const EBMRepository&) = delete;
        EBMRepository& operator=(const EBMRepository&) = delete;
        EBMRepository(EBMRepository&&) = delete;
        EBMRepository& operator=(EBMRepository&&) = delete;

        void clear() {
            ident_source = ReferenceSource{};
            identifier_repo.clear();
            string_repo.clear();
            type_repo.clear();
            expression_repo.clear();
            statement_repo.clear();
            debug_locs.clear();
        }

        // after this call, getter functions will returns nullptr
        // before reuse it, you should call clear()
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

        expected<ebm::IdentifierRef> anonymous_identifier() {
            return identifier_repo.new_id(ident_source);
        }

        expected<ebm::StatementRef> new_statement_id() {
            return statement_repo.new_id(ident_source);
        }

        ReferenceSource& get_identifier_source() {
            return ident_source;
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
            if (body.type.id.value() == 0) {
                return unexpect_error("Expression type is not set: {}", to_string(body.op));
            }
            return expression_repo.add(ident_source, std::move(body));
        }

        ebm::Statement* get_statement(const ebm::StatementRef& ref) {
            return statement_repo.get(ref);
        }

        ebm::Expression* get_expression(const ebm::ExpressionRef& ref) {
            return expression_repo.get(ref);
        }

        ebm::Type* get_type(const ebm::TypeRef& ref) {
            return type_repo.get(ref);
        }
    };

    struct ConverterContext {
       private:
        std::shared_ptr<StatementConverter> statement_converter;
        std::shared_ptr<ExpressionConverter> expression_converter;
        std::shared_ptr<EncoderConverter> encoder_converter;
        std::shared_ptr<DecoderConverter> decoder_converter;
        std::shared_ptr<TypeConverter> type_converter;

        EBMRepository repo_;
        ConverterState state_;

       public:
        ConverterContext();

        EBMRepository& repository() {
            return repo_;
        }

        ConverterState& state() {
            return state_;
        }

        StatementConverter& get_statement_converter();

        ExpressionConverter& get_expression_converter();

        EncoderConverter& get_encoder_converter();

        DecoderConverter& get_decoder_converter();

        TypeConverter& get_type_converter();

        // shorthand for creating a type with a single kind
        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);
        expected<ebm::StatementRef> convert_statement(ebm::StatementRef, const std::shared_ptr<ast::Node>& node);
        expected<ebm::ExpressionRef> convert_expr(const std::shared_ptr<ast::Expr>& node);
        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);
    };

    expected<ebm::TypeRef> get_counter_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_unsigned_n_int(ConverterContext& ctx, size_t n);
    expected<ebm::TypeRef> get_u8_n_array(ConverterContext& ctx, size_t n);
    expected<ebm::TypeRef> get_bool_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_void_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_encoder_return_type(ConverterContext& ctx);
    expected<ebm::TypeRef> get_decoder_return_type(ConverterContext& ctx);
    expected<ebm::ExpressionRef> get_int_literal(ConverterContext& ctx, std::uint64_t value);
    ebm::ExpressionBody get_int_literal_body(ebm::TypeRef type, std::uint64_t value);

    expected<std::string> decode_base64(const std::shared_ptr<ast::StrLiteral>& lit);

    struct StatementConverter {
        ConverterContext& ctx;
        expected<ebm::StatementRef> convert_statement(const std::shared_ptr<ast::Node>& node);

        expected<ebm::StructDecl> convert_struct_decl(ebm::IdentifierRef name, const std::shared_ptr<ast::StructType>& node);
        expected<ebm::StatementRef> convert_statement(ebm::StatementRef ref, const std::shared_ptr<ast::Node>& node);

       private:
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Assert>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Return>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Break>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Continue>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::If>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Loop>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Match>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::IndentBlock>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::MatchBranch>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Program>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Format>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Enum>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::EnumMember>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Function>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Metadata>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::State>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Field>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::ExplicitError>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Import>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::ImplicitYield>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Binary>& node, ebm::StatementRef id, ebm::StatementBody& body);
        expected<void> convert_statement_impl(const std::shared_ptr<ast::ScopedStatement>& node, ebm::StatementRef id, ebm::StatementBody& body);
        // Fallback for unhandled types
        expected<void> convert_statement_impl(const std::shared_ptr<ast::Node>& node, ebm::StatementRef id, ebm::StatementBody& body);

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
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Paren>& node, ebm::ExpressionBody& body);
        // Fallback for unhandled types
        expected<void> convert_expr_impl(const std::shared_ptr<ast::Expr>& node, ebm::ExpressionBody& body);
    };

    expected<ebm::ExpressionRef> get_alignment_requirement(ConverterContext& ctx, std::uint64_t alignment_bytes, ebm::StreamType type);

    struct EncoderConverter {
        ConverterContext& ctx;
        expected<ebm::StatementBody> encode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);

       private:
        expected<void> encode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> encode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> encode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<void> encode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<void> encode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> encode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);

        // internally, this method casts base_ref to unsigned n int type if necessary
        // so no need to cast it before calling this method
        expected<ebm::StatementRef> encode_multi_byte_int_with_fixed_array(size_t n, ebm::IOAttribute endian, ebm::ExpressionRef from, ebm::TypeRef cast_from);
    };

    struct DecoderConverter {
        ConverterContext& ctx;
        expected<ebm::StatementBody> decode_field_type(const std::shared_ptr<ast::Type>& typ, ebm::ExpressionRef base_ref, const std::shared_ptr<ast::Field>& field);

       private:
        expected<void> decode_int_type(ebm::IOData& io_desc, const std::shared_ptr<ast::IntType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> decode_float_type(ebm::IOData& io_desc, const std::shared_ptr<ast::FloatType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> decode_enum_type(ebm::IOData& io_desc, const std::shared_ptr<ast::EnumType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<void> decode_array_type(ebm::IOData& io_desc, const std::shared_ptr<ast::ArrayType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<void> decode_str_literal_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StrLiteralType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts);
        expected<void> decode_struct_type(ebm::IOData& io_desc, const std::shared_ptr<ast::StructType>& typ, ebm::ExpressionRef base_ref, ebm::LoweredStatements& lowered_stmts, const std::shared_ptr<ast::Field>& field);
        expected<ebm::StatementRef> decode_multi_byte_int_with_fixed_array(size_t n, ebm::IOAttribute endian, ebm::ExpressionRef to, ebm::TypeRef cast_to);
    };

    struct TypeConverter {
        ConverterContext& ctx;
        expected<ebm::TypeRef> convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field = nullptr);
        expected<ebm::TypeBody> convert_function_type(const std::shared_ptr<ast::FunctionType>& type);

        expected<ebm::CastType> get_cast_type(ebm::TypeRef dest, ebm::TypeRef src);
    };

    expected<ebm::StatementRef> add_endian_specific(ConverterContext& ctx, ebm::IOAttribute endian, std::function<expected<ebm::StatementRef>()> on_little_endian, std::function<expected<ebm::StatementRef>()> on_big_endian);
    expected<void> construct_string_array(ConverterContext& ctx, ebm::Block& block, ebm::ExpressionRef n_array, const std::string& candidate);

    expected<ebm::StatementBody> assert_statement_body(ConverterContext& ctx, ebm::ExpressionRef condition);
    expected<ebm::StatementRef> assert_statement(ConverterContext& ctx, ebm::ExpressionRef condition);

    expected<ebm::BinaryOp> convert_assignment_binary_op(ast::BinaryOp op);
    expected<ebm::BinaryOp> convert_binary_op(ast::BinaryOp op);

    struct TransformContext {
       private:
        ConverterContext& ctx;

       public:
        TransformContext(ConverterContext& ctx)
            : ctx(ctx) {}

        auto& type_repository() {
            return ctx.repository().type_repo;
        }

        auto& statement_repository() {
            return ctx.repository().statement_repo;
        }

        auto& expression_repository() {
            return ctx.repository().expression_repo;
        }

        auto& identifier_repository() {
            return ctx.repository().identifier_repo;
        }

        auto& string_repository() {
            return ctx.repository().string_repo;
        }

        auto& alias_vector() {
            return ctx.repository().aliases;
        }

        auto max_id() const {
            return ctx.repository().ident_source.current_id();
        }

        void set_max_id(std::uint64_t id) {
            ctx.repository().ident_source.set_current_id(id);
        }

        expected<ebm::AnyRef> new_id() {
            return ctx.repository().ident_source.new_id().transform([](ebm::Varint id) {
                return ebm::AnyRef{id};
            });
        }

        auto& context() {
            return ctx;
        }
    };
}  // namespace ebmgen
