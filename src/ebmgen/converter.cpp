#include "converter.hpp"
#include <core/ast/traverse.h>
#include <functional>
#include "convert/helper.hpp"
#include "ebm/extended_binary_module.hpp"
namespace ebmgen {

    expected<ebm::EndianExpr> ConverterState::get_endian(ebm::Endian base, bool sign) {
        ebm::EndianExpr e;
        e.sign(sign);
        e.endian(base);
        if (base != ebm::Endian::unspec) {
            return e;
        }
        e.endian(global_endian);
        if (on_function) {
            e.endian(local_endian);
        }
        if (e.endian() == ebm::Endian::dynamic) {
            e.dynamic_ref = current_dynamic_endian;
        }
        return e;
    }

    bool ConverterState::set_endian(ebm::Endian e, ebm::StatementRef id) {
        if (on_function) {
            local_endian = e;
            current_dynamic_endian = id;
            return true;
        }
        if (e == ebm::Endian::dynamic) {
            return false;
        }
        global_endian = e;
        return true;
    }

    expected<void> EBMRepository::finalize(ebm::ExtendedBinaryModule& ebm) {
        MAYBE(max_id, ident_source.current_id());
        ebm.max_id = ebm::AnyRef{.id = max_id};
        MAYBE(identifiers_len, varint(identifier_repo.get_all().size()));
        ebm.identifiers_len = identifiers_len;
        ebm.identifiers = std::move(identifier_repo.get_all());

        MAYBE(strings_len, varint(string_repo.get_all().size()));
        ebm.strings_len = strings_len;
        ebm.strings = std::move(string_repo.get_all());

        MAYBE(types_len, varint(type_repo.get_all().size()));
        ebm.types_len = types_len;
        ebm.types = std::move(type_repo.get_all());

        MAYBE(statements_len, varint(statement_repo.get_all().size()));
        ebm.statements_len = statements_len;
        ebm.statements = std::move(statement_repo.get_all());

        MAYBE(expressions_len, varint(expression_repo.get_all().size()));
        ebm.expressions_len = expressions_len;
        ebm.expressions = std::move(expression_repo.get_all());

        MAYBE(loc_len, varint(debug_locs.size()));

        ebm.debug_info.locs = std::move(debug_locs);
        ebm.debug_info.len_locs = loc_len;

        return {};
    }

    ConverterContext::ConverterContext() {
        statement_converter = std::make_shared<StatementConverter>(*this);
        expression_converter = std::make_shared<ExpressionConverter>(*this);
        encoder_converter = std::make_shared<EncoderConverter>(*this);
        decoder_converter = std::make_shared<DecoderConverter>(*this);
        type_converter = std::make_shared<TypeConverter>(*this);
    }

    StatementConverter& ConverterContext::get_statement_converter() {
        return *statement_converter;
    }

    ExpressionConverter& ConverterContext::get_expression_converter() {
        return *expression_converter;
    }

    EncoderConverter& ConverterContext::get_encoder_converter() {
        return *encoder_converter;
    }

    DecoderConverter& ConverterContext::get_decoder_converter() {
        return *decoder_converter;
    }

    TypeConverter& ConverterContext::get_type_converter() {
        return *type_converter;
    }

    expected<ebm::StatementRef> ConverterContext::convert_statement(const std::shared_ptr<ast::Node>& node) {
        return statement_converter->convert_statement(node);
    }
    expected<ebm::ExpressionRef> ConverterContext::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        return expression_converter->convert_expr(node);
    }
    expected<ebm::TypeRef> ConverterContext::convert_type(const std::shared_ptr<ast::Type>& type, const std::shared_ptr<ast::Field>& field) {
        return type_converter->convert_type(type, field);
    }

    expected<ebm::StatementRef> add_endian_specific(ConverterContext& ctx, ebm::EndianExpr endian, std::function<expected<ebm::StatementRef>()> on_little_endian, std::function<expected<ebm::StatementRef>()> on_big_endian) {
        ebm::StatementRef ref;
        const auto is_native_or_dynamic = endian.endian() == ebm::Endian::native || endian.endian() == ebm::Endian::dynamic;
        if (is_native_or_dynamic) {
            ebm::ExpressionBody is_little;
            is_little.op = ebm::ExpressionOp::IS_LITTLE_ENDIAN;
            is_little.endian_expr(endian.dynamic_ref);  // if native, this will be empty
            EBMA_ADD_EXPR(is_little_ref, std::move(is_little));
            MAYBE(then_block, on_little_endian());
            MAYBE(else_block, on_big_endian());
            EBM_IF_STATEMENT(res, is_little_ref, then_block, else_block);
            ref = res;
        }
        else if (endian.endian() == ebm::Endian::little) {
            MAYBE(res, on_little_endian());
            ref = res;
        }
        else if (endian.endian() == ebm::Endian::big) {
            MAYBE(res, on_big_endian());
            ref = res;
        }
        else {
            return unexpect_error("Unsupported endian type: {}", to_string(endian.endian()));
        }
        return ref;
    }

    expected<ebm::TypeRef> get_unsigned_n_int(ConverterContext& ctx, size_t n) {
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::UINT;
        typ.size(n);
        EBMA_ADD_TYPE(type_ref, std::move(typ));
        return type_ref;
    }

    expected<ebm::TypeRef> get_counter_type(ConverterContext& ctx) {
        return get_unsigned_n_int(ctx, 64);
    }

    expected<ebm::ExpressionRef> get_int_literal(ConverterContext& ctx, std::uint64_t value) {
        ebm::ExpressionBody body;
        body.op = ebm::ExpressionOp::LITERAL_INT;
        body.int_value(value);
        EBMA_ADD_EXPR(int_literal, std::move(body));
        return int_literal;
    }

    expected<ebm::TypeRef> get_single_type(ebm::TypeKind kind, ConverterContext& ctx) {
        ebm::TypeBody typ;
        typ.kind = kind;
        EBMA_ADD_TYPE(type_ref, std::move(typ));
        return type_ref;
    }

    expected<ebm::TypeRef> get_bool_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::BOOL, ctx);
    }

    expected<ebm::TypeRef> get_void_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::VOID, ctx);
    }

    expected<ebm::TypeRef> get_encoder_return_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::ENCODER_RETURN, ctx);
    }
    expected<ebm::TypeRef> get_decoder_return_type(ConverterContext& ctx) {
        return get_single_type(ebm::TypeKind::DECODER_RETURN, ctx);
    }

    expected<ebm::TypeRef> get_u8_n_array(ConverterContext& ctx, size_t n) {
        EBMU_U8(u8typ);
        ebm::TypeBody typ;
        typ.kind = ebm::TypeKind::ARRAY;
        typ.element_type(u8typ);
        typ.length(*varint(n));
        EBMA_ADD_TYPE(type_ref, std::move(typ));
        return type_ref;
    }

}  // namespace ebmgen
