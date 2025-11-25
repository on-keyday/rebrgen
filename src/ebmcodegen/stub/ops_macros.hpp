// Auto-generated file, do not edit directly.
#pragma once
#include <ebm/extended_binary_module.hpp>
#define APPLY_BINARY_OP(op,...) \
    if(op == ebm::BinaryOp::mul) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)*(b); });  \
    if(op == ebm::BinaryOp::div) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)/(b); });  \
    if(op == ebm::BinaryOp::mod) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)%(b); });  \
    if(op == ebm::BinaryOp::left_shift) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)<<(b); });  \
    if(op == ebm::BinaryOp::right_shift) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)>>(b); });  \
    if(op == ebm::BinaryOp::bit_and) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)&(b); });  \
    if(op == ebm::BinaryOp::add) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)+(b); });  \
    if(op == ebm::BinaryOp::sub) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)-(b); });  \
    if(op == ebm::BinaryOp::bit_or) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)|(b); });  \
    if(op == ebm::BinaryOp::bit_xor) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)^(b); });  \
    if(op == ebm::BinaryOp::equal) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)==(b); });  \
    if(op == ebm::BinaryOp::not_equal) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)!=(b); });  \
    if(op == ebm::BinaryOp::less) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)<(b); });  \
    if(op == ebm::BinaryOp::less_or_eq) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)<=(b); });  \
    if(op == ebm::BinaryOp::greater) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)>(b); });  \
    if(op == ebm::BinaryOp::greater_or_eq) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)>=(b); });  \
    if(op == ebm::BinaryOp::logical_and) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)&&(b); });  \
    if(op == ebm::BinaryOp::logical_or) return (__VA_ARGS__)([](auto&& a, auto&& b) { return (a)||(b); }); 
#define APPLY_UNARY_OP(op,...) \
    if(op == ebm::UnaryOp::logical_not) return (__VA_ARGS__)([](auto&& a) { return !(a); }); \
    if(op == ebm::UnaryOp::minus_sign) return (__VA_ARGS__)([](auto&& a) { return -(a); }); \
    if(op == ebm::UnaryOp::bit_not) return (__VA_ARGS__)([](auto&& a) { return ~(a); });
namespace ebm {
constexpr auto BinaryOp_to_OpCode(BinaryOp op) -> OpCode {
    switch(op) {
        case BinaryOp::mul: return OpCode::MUL;
        case BinaryOp::div: return OpCode::DIV;
        case BinaryOp::mod: return OpCode::MOD;
        case BinaryOp::left_shift: return OpCode::LSHIFT;
        case BinaryOp::right_shift: return OpCode::RSHIFT;
        case BinaryOp::bit_and: return OpCode::BIT_AND;
        case BinaryOp::add: return OpCode::ADD;
        case BinaryOp::sub: return OpCode::SUB;
        case BinaryOp::bit_or: return OpCode::BIT_OR;
        case BinaryOp::bit_xor: return OpCode::BIT_XOR;
        case BinaryOp::equal: return OpCode::EQ;
        case BinaryOp::not_equal: return OpCode::NEQ;
        case BinaryOp::less: return OpCode::LT;
        case BinaryOp::less_or_eq: return OpCode::LE;
        case BinaryOp::greater: return OpCode::GT;
        case BinaryOp::greater_or_eq: return OpCode::GE;
        case BinaryOp::logical_and: return OpCode::LOGICAL_AND;
        case BinaryOp::logical_or: return OpCode::LOGICAL_OR;
    }
    return OpCode::NOP;
}
constexpr auto UnaryOp_to_OpCode(UnaryOp op) -> OpCode {
    switch(op) {
        case UnaryOp::logical_not: return OpCode::LOGICAL_NOT;
        case UnaryOp::minus_sign: return OpCode::NEG;
        case UnaryOp::bit_not: return OpCode::BIT_NOT;
    }
    return OpCode::NOP;
}
} // namespace ebm
