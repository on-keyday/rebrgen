
#include <gtest/gtest.h>
#include "ebmgen/converter.hpp"  // For now, include cpp to access static functions

TEST(ExpressionTest, ConvertBinaryOp) {
    auto op = ebmgen::convert_binary_op(brgen::ast::BinaryOp::add);
    ASSERT_TRUE(op.has_value());
    EXPECT_EQ(op.value(), ebm::BinaryOp::add);
}
