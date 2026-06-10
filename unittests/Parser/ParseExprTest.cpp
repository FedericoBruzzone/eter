//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "eter/Parser/NodePool.h"
#include "eter/Parser/Parser.h"
#include "eter/Parser/Regime.h"

#include <llvm/Support/VirtualFileSystem.h>

#include "TestParserHelper.h"
#include "gtest/gtest.h"

using namespace eter;
using namespace eter::parser;
using namespace eter::lexer;
using namespace ParserTestHelper;

using EToken = eter::lexer::Token;

// Helpers to reduce boilerplate in expression tests.
static NodeIndex fnBody() {
  return PR.Pool.childrenOf(PR.Pool.childrenOf(PR.Root)[0])[1];
}
static NodeIndex firstLet() { return PR.Pool.childrenOf(fnBody())[0]; }

TEST(ParserTestExpr, LetBindingSimpleLiteralExpr) {
  parseSource("fn main(){ let imm x: i32 = 10; }");

  EXPECT_TRUE(PR.ok());
  EXPECT_TRUE(checkChildrenKinds(PR.Root, NodeKind::FnDecl));
  const NodeIndex Fn = PR.Pool.childrenOf(PR.Root)[0];
  EXPECT_TRUE(checkChildrenKinds(Fn, NodeKind::ParamList, NodeKind::BlockExpr));

  const NodeIndex Body = PR.Pool.childrenOf(Fn)[1];
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::LetStmt));

  const NodeIndex Let = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "i32");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "10");
}

TEST(ParserTestExpr, LetMutWithType) {
  parseSource("fn main(){ let mut x: i32 = 10; }");

  EXPECT_TRUE(PR.ok());
  EXPECT_TRUE(checkChildrenKinds(PR.Root, NodeKind::FnDecl));

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Mut);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "i32");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "10");
}

TEST(ParserTestExpr, LetProjWithType) {
  parseSource("fn main(){ let proj x: i32 = 10; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Proj);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "i32");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "10");
}

TEST(ParserTestExpr, LetWithoutRegime) {
  parseSource("fn main(){ let x: i32 = 10; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::None);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "i32");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "10");
}

TEST(ParserTestExpr, LetWithBoolLit) {
  parseSource("fn main(){ let imm x: bool = true; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "bool");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "true");
}

TEST(ParserTestExpr, LetWithStringLit) {
  parseSource(R"(fn main(){ let imm x: str = "hello"; })");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "str");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "\"hello\"");
}

TEST(ParserTestExpr, LetWithFloatLit) {
  parseSource("fn main(){ let mut y: f32 = 3.14; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "y");
  checkRegime(Let, Regime::Mut);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "f32");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "3.14");
}

TEST(ParserTestExpr, LetWithCharLit) {
  parseSource("fn main(){ let imm c: char = 'a'; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "c");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "char");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "'a'");
}

TEST(ParserTestExpr, LetWithSimpleBinary) {
  parseSource("fn main(){ let imm x: i32 = 1 + 2; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);

  const NodeIndex Rhs = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Rhs].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(checkChildrenKinds(Rhs, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Rhs)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Rhs)[1], "2");
}

TEST(ParserTestExpr, LetWithBinaryPrecedence) {
  parseSource("fn main(){ let imm x: i32 = 1 + 2 * 3; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));
  checkInternedString(Let, "x");

  // 1 + 2 * 3  =>  1 + (2 * 3)
  const NodeIndex Outer = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Outer].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(
      checkChildrenKinds(Outer, NodeKind::LitExpr, NodeKind::BinaryExpr));
  checkInternedString(PR.Pool.childrenOf(Outer)[0], "1");

  const NodeIndex Inner = PR.Pool.childrenOf(Outer)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Inner].Payload),
            static_cast<uint16_t>(EToken::Kind::star));
  EXPECT_TRUE(checkChildrenKinds(Inner, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Inner)[0], "2");
  checkInternedString(PR.Pool.childrenOf(Inner)[1], "3");
}

TEST(ParserTestExpr, LetWithParenOverride) {
  parseSource("fn main(){ let imm x: i32 = (1 + 2) * 3; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));
  checkInternedString(Let, "x");

  // (1 + 2) * 3  =>  (1+2) * 3, parens elided, outer is *
  const NodeIndex Outer = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Outer].Payload),
            static_cast<uint16_t>(EToken::Kind::star));
  EXPECT_TRUE(
      checkChildrenKinds(Outer, NodeKind::BinaryExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Outer)[1], "3");

  // Inner is the (1+2) sub-tree
  const NodeIndex Inner = PR.Pool.childrenOf(Outer)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Inner].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(checkChildrenKinds(Inner, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Inner)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Inner)[1], "2");
}

TEST(ParserTestExpr, LetWithLeftAssoc) {
  parseSource("fn main(){ let imm x: i32 = 1 + 2 - 3; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);

  // 1 + 2 - 3  =>  (1 + 2) - 3  (left-associative)
  const NodeIndex Outer = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Outer].Payload),
            static_cast<uint16_t>(EToken::Kind::minus));
  EXPECT_TRUE(
      checkChildrenKinds(Outer, NodeKind::BinaryExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Outer)[1], "3");

  const NodeIndex Inner = PR.Pool.childrenOf(Outer)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Inner].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(checkChildrenKinds(Inner, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Inner)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Inner)[1], "2");
}

TEST(ParserTestExpr, LetWithUnaryMinus) {
  parseSource("fn main(){ let imm x: i32 = -5; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::UnaryExpr));
  checkInternedString(Let, "x");

  const NodeIndex Rhs = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Rhs].Payload),
            static_cast<uint16_t>(EToken::Kind::minus));
  EXPECT_TRUE(checkChildrenKinds(Rhs, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Rhs)[0], "5");
}

TEST(ParserTestExpr, LetWithUnaryNot) {
  parseSource("fn main(){ let imm x: bool = !true; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::UnaryExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);

  const NodeIndex Rhs = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Rhs].Payload),
            static_cast<uint16_t>(EToken::Kind::bang));
  EXPECT_TRUE(checkChildrenKinds(Rhs, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Rhs)[0], "true");
}

TEST(ParserTestExpr, MultipleLetBindings) {
  parseSource("fn main(){ let imm x: i32 = 10; let mut y: bool = false; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::LetStmt, NodeKind::LetStmt));

  const NodeIndex Let1 = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(checkChildrenKinds(Let1, NodeKind::NamedType, NodeKind::LitExpr));

  checkInternedString(Let1, "x");
  checkRegime(Let1, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let1)[1], "10");

  const NodeIndex Let2 = PR.Pool.childrenOf(Body)[1];
  EXPECT_TRUE(checkChildrenKinds(Let2, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let2, "y");
  checkRegime(Let2, Regime::Mut);
  checkInternedString(PR.Pool.childrenOf(Let2)[1], "false");
}

TEST(ParserTestExpr, NestedBlockExpr) {
  parseSource("fn main(){ { let imm x: i32 = 10; } }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  // Outer block has one child: the inner block
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::BlockExpr));

  const NodeIndex InnerBlock = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(checkChildrenKinds(InnerBlock, NodeKind::LetStmt));

  const NodeIndex Let = PR.Pool.childrenOf(InnerBlock)[0];
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "i32");
  checkInternedString(PR.Pool.childrenOf(Let)[1], "10");
}

TEST(ParserTestExpr, ExprStmtLiteral) {
  parseSource("fn main() { 42; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Body)[0], "42");
}

TEST(ParserTestExpr, ExprStmtBinary) {
  parseSource("fn main() { 1 + 2; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::BinaryExpr));

  const NodeIndex Expr = PR.Pool.childrenOf(Body)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Expr].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(checkChildrenKinds(Expr, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Expr)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Expr)[1], "2");
}

TEST(ParserTestExpr, ExprStmtParen) {
  parseSource("fn main() { (1 + 2); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  // Parens are elided; the inner expression is a direct child
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::BinaryExpr));

  const NodeIndex Expr = PR.Pool.childrenOf(Body)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Expr].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(checkChildrenKinds(Expr, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Expr)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Expr)[1], "2");
}

TEST(ParserTestExpr, ExprStmtUnary) {
  parseSource("fn main() { -5; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::UnaryExpr));

  const NodeIndex Expr = PR.Pool.childrenOf(Body)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Expr].Payload),
            static_cast<uint16_t>(EToken::Kind::minus));
  EXPECT_TRUE(checkChildrenKinds(Expr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Expr)[0], "5");
}

TEST(ParserTestExpr, LetWithMulAndDiv) {
  parseSource("fn main(){ let imm x: i32 = 1 * 2 / 3; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);

  // 1 * 2 / 3  =>  (1 * 2) / 3
  const NodeIndex Outer = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Outer].Payload),
            static_cast<uint16_t>(EToken::Kind::slash));
  EXPECT_TRUE(
      checkChildrenKinds(Outer, NodeKind::BinaryExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Outer)[1], "3");

  const NodeIndex Inner = PR.Pool.childrenOf(Outer)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Inner].Payload),
            static_cast<uint16_t>(EToken::Kind::star));
  EXPECT_TRUE(checkChildrenKinds(Inner, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Inner)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Inner)[1], "2");
}

TEST(ParserTestExpr, LetWithComplexParens) {
  parseSource("fn main(){ let imm x: i32 = (1 + 2) * (3 - 4) / 5; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);

  // (1 + 2) * (3 - 4) / 5 => (((1 + 2) * (3 - 4)) / 5)
  // Top level is /
  const NodeIndex Outer = PR.Pool.childrenOf(Let)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Outer].Payload),
            static_cast<uint16_t>(EToken::Kind::slash));
  EXPECT_TRUE(
      checkChildrenKinds(Outer, NodeKind::BinaryExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Outer)[1], "5");

  // Middle is *
  const NodeIndex Mul = PR.Pool.childrenOf(Outer)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Mul].Payload),
            static_cast<uint16_t>(EToken::Kind::star));
  EXPECT_TRUE(
      checkChildrenKinds(Mul, NodeKind::BinaryExpr, NodeKind::BinaryExpr));

  // Left is (1 + 2)
  const NodeIndex Plus = PR.Pool.childrenOf(Mul)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Plus].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(checkChildrenKinds(Plus, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Plus)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Plus)[1], "2");

  // Right is (3 - 4)
  const NodeIndex Minus = PR.Pool.childrenOf(Mul)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Minus].Payload),
            static_cast<uint16_t>(EToken::Kind::minus));
  EXPECT_TRUE(checkChildrenKinds(Minus, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Minus)[0], "3");
  checkInternedString(PR.Pool.childrenOf(Minus)[1], "4");
}

TEST(ParserTestExpr, ExprStmtComplexCombo) {
  parseSource("fn main() { 10 / 2 + 3 * 4; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::BinaryExpr));

  // 10 / 2 + 3 * 4 => (10 / 2) + (3 * 4)
  const NodeIndex Outer = PR.Pool.childrenOf(Body)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Outer].Payload),
            static_cast<uint16_t>(EToken::Kind::plus));
  EXPECT_TRUE(
      checkChildrenKinds(Outer, NodeKind::BinaryExpr, NodeKind::BinaryExpr));

  const NodeIndex Div = PR.Pool.childrenOf(Outer)[0];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Div].Payload),
            static_cast<uint16_t>(EToken::Kind::slash));
  EXPECT_TRUE(checkChildrenKinds(Div, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Div)[0], "10");
  checkInternedString(PR.Pool.childrenOf(Div)[1], "2");

  const NodeIndex Mul = PR.Pool.childrenOf(Outer)[1];
  EXPECT_EQ(NodePool::payloadOp(PR.Pool[Mul].Payload),
            static_cast<uint16_t>(EToken::Kind::star));
  EXPECT_TRUE(checkChildrenKinds(Mul, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Mul)[0], "3");
  checkInternedString(PR.Pool.childrenOf(Mul)[1], "4");
}

//===----------------------------------------------------------------------===//
// Postfix expression tests
//===----------------------------------------------------------------------===//

TEST(ParserTestExpr, ExprStmtCallNoArgs) {
  parseSource("fn foo(){} fn main() { foo(); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = PR.Pool.childrenOf(PR.Pool.childrenOf(PR.Root)[1])[1];
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::CallExpr));

  const NodeIndex Call = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(checkChildrenKinds(Call, NodeKind::IdentExpr, NodeKind::ArgList));

  const NodeIndex Callee = PR.Pool.childrenOf(Call)[0];
  EXPECT_EQ(PR.Pool.kindOf(Callee), NodeKind::IdentExpr);
  checkInternedString(Callee, "foo");

  const NodeIndex Args = PR.Pool.childrenOf(Call)[1];
  EXPECT_EQ(PR.Pool.kindOf(Args), NodeKind::ArgList);
  EXPECT_EQ(PR.Pool.childrenOf(Args).size(), 0u);
}

TEST(ParserTestExpr, ExprStmtCallOneArg) {
  parseSource("fn foo(mut x: i32){} fn main() { foo(42); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = PR.Pool.childrenOf(PR.Pool.childrenOf(PR.Root)[1])[1];
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::CallExpr));

  const NodeIndex Call = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(checkChildrenKinds(Call, NodeKind::IdentExpr, NodeKind::ArgList));
  checkInternedString(PR.Pool.childrenOf(Call)[0], "foo");

  const NodeIndex Args = PR.Pool.childrenOf(Call)[1];
  EXPECT_TRUE(checkChildrenKinds(Args, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Args)[0], "42");
}

TEST(ParserTestExpr, ExprStmtCallMultipleArgs) {
  parseSource(" fn foo(mut x: i32, mut y: i32, mut z: i32){} fn main() { let "
              "imm foo: i32 = 42; foo(1, 2, 3); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = PR.Pool.childrenOf(PR.Pool.childrenOf(PR.Root)[1])[1];
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::LetStmt, NodeKind::CallExpr));

  const NodeIndex Call = PR.Pool.childrenOf(Body)[1];
  EXPECT_TRUE(checkChildrenKinds(Call, NodeKind::IdentExpr, NodeKind::ArgList));
  checkInternedString(PR.Pool.childrenOf(Call)[0], "foo");

  const NodeIndex Args = PR.Pool.childrenOf(Call)[1];
  EXPECT_TRUE(checkChildrenKinds(Args, NodeKind::LitExpr, NodeKind::LitExpr,
                                 NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Args)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Args)[1], "2");
  checkInternedString(PR.Pool.childrenOf(Args)[2], "3");
}

TEST(ParserTestExpr, LetWithoutInit) {
  parseSource("fn main(){ let imm x: i32; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  // Only type child, no init expression
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType));
  EXPECT_EQ(PR.Pool.childrenOf(Let).size(), 1u);
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "i32");
}

TEST(ParserTestExpr, LetWithCallExpr) {
  parseSource("fn foo(x: i32, y: i32): i32 {ret x+y;} "
              "fn main(){ let imm x: i32 = foo(1, 2); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = PR.Pool.childrenOf(PR.Pool.childrenOf(PR.Root)[1])[1];
  const NodeIndex Let = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::CallExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);

  const NodeIndex Call = PR.Pool.childrenOf(Let)[1];
  EXPECT_TRUE(checkChildrenKinds(Call, NodeKind::IdentExpr, NodeKind::ArgList));
  checkInternedString(PR.Pool.childrenOf(Call)[0], "foo");

  const NodeIndex Args = PR.Pool.childrenOf(Call)[1];
  EXPECT_TRUE(checkChildrenKinds(Args, NodeKind::LitExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Args)[0], "1");
  checkInternedString(PR.Pool.childrenOf(Args)[1], "2");
}

//===----------------------------------------------------------------------===//
// Let without type annotation
//===----------------------------------------------------------------------===//

TEST(ParserTestExpr, LetWithoutTypeAnnotation) {
  parseSource("fn main(){ let imm x = 10; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::LitExpr));
  checkInternedString(Let, "x");
  checkRegime(Let, Regime::Imm);
  checkInternedString(PR.Pool.childrenOf(Let)[0], "10");
}

//===----------------------------------------------------------------------===//
// Struct literals
//===----------------------------------------------------------------------===//

TEST(ParserTestExpr, StructLitSingleField) {
  parseSource("fn main(){ let imm p = X { x: 42 }; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::StructLitExpr));

  const NodeIndex Lit = PR.Pool.childrenOf(Let)[0];
  checkInternedString(Lit, "X");
  checkSpan(Lit, "X { x: 42 }");
  EXPECT_TRUE(checkChildrenKinds(Lit, NodeKind::FieldInit));

  const NodeIndex Init = PR.Pool.childrenOf(Lit)[0];
  checkInternedString(Init, "x");
  EXPECT_TRUE(checkChildrenKinds(Init, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Init)[0], "42");
}

TEST(ParserTestExpr, StructLitEmpty) {
  parseSource("fn main(){ let imm p = X {}; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Lit = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_EQ(PR.Pool.kindOf(Lit), NodeKind::StructLitExpr);
  checkInternedString(Lit, "X");
  EXPECT_EQ(PR.Pool.childrenOf(Lit).size(), 0u);
}

TEST(ParserTestExpr, StructLitTrailingComma) {
  parseSource("fn main(){ let imm p = X { x: 1, y: 2, }; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Lit = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_TRUE(
      checkChildrenKinds(Lit, NodeKind::FieldInit, NodeKind::FieldInit));
  checkInternedString(PR.Pool.childrenOf(Lit)[0], "x");
  checkInternedString(PR.Pool.childrenOf(Lit)[1], "y");
}

TEST(ParserTestExpr, StructLitShorthand) {
  // `X { x }` is shorthand for `X { x: x }`: the parser synthesises the
  // IdentExpr child.
  parseSource("fn main(){ let imm p = X { x }; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Lit = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_TRUE(checkChildrenKinds(Lit, NodeKind::FieldInit));

  const NodeIndex Init = PR.Pool.childrenOf(Lit)[0];
  checkInternedString(Init, "x");
  EXPECT_TRUE(checkChildrenKinds(Init, NodeKind::IdentExpr));
  checkInternedString(PR.Pool.childrenOf(Init)[0], "x");
}

TEST(ParserTestExpr, StructLitNested) {
  parseSource("fn main(){ let imm p = X { y: Y { q: 1 } }; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Outer = PR.Pool.childrenOf(firstLet())[0];
  checkInternedString(Outer, "X");

  const NodeIndex InitY = PR.Pool.childrenOf(Outer)[0];
  EXPECT_TRUE(checkChildrenKinds(InitY, NodeKind::StructLitExpr));

  const NodeIndex Inner = PR.Pool.childrenOf(InitY)[0];
  checkInternedString(Inner, "Y");
  EXPECT_TRUE(checkChildrenKinds(Inner, NodeKind::FieldInit));
}

TEST(ParserTestExpr, StructLitFieldAccess) {
  parseSource("fn main(){ let imm v = X { x: 1 }.x; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Field = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_EQ(PR.Pool.kindOf(Field), NodeKind::FieldExpr);
  EXPECT_TRUE(checkChildrenKinds(Field, NodeKind::StructLitExpr));
}

TEST(ParserTestExpr, IfCondIsNotStructLit) {
  // In an `if` header the `{` opens the then-block, not a struct literal.
  parseSource("fn main(){ if x { } }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex If = PR.Pool.childrenOf(fnBody())[0];
  EXPECT_EQ(PR.Pool.kindOf(If), NodeKind::IfExpr);
  EXPECT_TRUE(checkChildrenKinds(If, NodeKind::IdentExpr, NodeKind::BlockExpr));
}

TEST(ParserTestExpr, WhileCondIsNotStructLit) {
  parseSource("fn main(){ while x { } }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex While = PR.Pool.childrenOf(fnBody())[0];
  EXPECT_EQ(PR.Pool.kindOf(While), NodeKind::WhileStmt);
  EXPECT_TRUE(
      checkChildrenKinds(While, NodeKind::IdentExpr, NodeKind::BlockExpr));
}

TEST(ParserTestExpr, ParenLiftsStructLitRestriction) {
  // Parentheses disambiguate, so a struct literal is legal again inside an
  // `if` header.
  parseSource("fn main(){ if (X { x: 1 }).x { } }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex If = PR.Pool.childrenOf(fnBody())[0];
  EXPECT_TRUE(checkChildrenKinds(If, NodeKind::FieldExpr, NodeKind::BlockExpr));
}

TEST(ParserTestExpr, StructLitAsCallArg) {
  parseSource("fn main(){ foo(X { x: 1 }); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Call = PR.Pool.childrenOf(fnBody())[0];
  EXPECT_TRUE(checkChildrenKinds(Call, NodeKind::IdentExpr, NodeKind::ArgList));
  const NodeIndex Args = PR.Pool.childrenOf(Call)[1];
  EXPECT_TRUE(checkChildrenKinds(Args, NodeKind::StructLitExpr));
}

//===----------------------------------------------------------------------===//
// Tuples
//===----------------------------------------------------------------------===//

TEST(ParserTestExpr, TupleExprAndType) {
  // The example from the language specification.
  parseSource(
      R"(fn main(){ let imm record: (i32, f64, str) = (42, 3.14, "hello"); })");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::TupleType, NodeKind::TupleExpr));

  const NodeIndex Ty = PR.Pool.childrenOf(Let)[0];
  checkSpan(Ty, "(i32, f64, str)");
  EXPECT_TRUE(checkChildrenKinds(Ty, NodeKind::NamedType, NodeKind::NamedType,
                                 NodeKind::NamedType));
  checkInternedString(PR.Pool.childrenOf(Ty)[0], "i32");
  checkInternedString(PR.Pool.childrenOf(Ty)[1], "f64");
  checkInternedString(PR.Pool.childrenOf(Ty)[2], "str");

  const NodeIndex Tup = PR.Pool.childrenOf(Let)[1];
  checkSpan(Tup, R"((42, 3.14, "hello"))");
  EXPECT_TRUE(checkChildrenKinds(Tup, NodeKind::LitExpr, NodeKind::LitExpr,
                                 NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Tup)[0], "42");
  checkInternedString(PR.Pool.childrenOf(Tup)[1], "3.14");
  checkInternedString(PR.Pool.childrenOf(Tup)[2], "\"hello\"");
}

TEST(ParserTestExpr, ParenExprIsNotTuple) {
  parseSource("fn main(){ let imm x = (42); }");

  EXPECT_TRUE(PR.ok());

  // `(x)` is a parenthesised expression: parens are elided, no tuple node.
  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::LitExpr));
}

TEST(ParserTestExpr, OneElementTupleNeedsTrailingComma) {
  parseSource("fn main(){ let imm t = (42,); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Tup = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_EQ(PR.Pool.kindOf(Tup), NodeKind::TupleExpr);
  EXPECT_TRUE(checkChildrenKinds(Tup, NodeKind::LitExpr));
}

TEST(ParserTestExpr, UnitTupleExprAndType) {
  parseSource("fn main(){ let imm u: () = (); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::TupleType, NodeKind::TupleExpr));
  EXPECT_EQ(PR.Pool.childrenOf(PR.Pool.childrenOf(Let)[0]).size(), 0u);
  EXPECT_EQ(PR.Pool.childrenOf(PR.Pool.childrenOf(Let)[1]).size(), 0u);
}

TEST(ParserTestExpr, NestedTuple) {
  parseSource("fn main(){ let imm t: (i32, (f64, str)) = ((1, 2), 3); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::TupleType, NodeKind::TupleExpr));

  const NodeIndex Ty = PR.Pool.childrenOf(Let)[0];
  EXPECT_TRUE(checkChildrenKinds(Ty, NodeKind::NamedType, NodeKind::TupleType));

  const NodeIndex Tup = PR.Pool.childrenOf(Let)[1];
  EXPECT_TRUE(checkChildrenKinds(Tup, NodeKind::TupleExpr, NodeKind::LitExpr));
}

TEST(ParserTestExpr, TupleExprTrailingComma) {
  parseSource("fn main(){ let imm t = (1, 2, ); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Tup = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_TRUE(checkChildrenKinds(Tup, NodeKind::LitExpr, NodeKind::LitExpr));
}

TEST(ParserTestExpr, TupleIndexAccess) {
  parseSource("fn main(){ let imm x = point.0; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Idx = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_EQ(PR.Pool.kindOf(Idx), NodeKind::TupleIndexExpr);
  checkInternedString(Idx, "0");
  checkSpan(Idx, "point.0");
  EXPECT_TRUE(checkChildrenKinds(Idx, NodeKind::IdentExpr));
  checkInternedString(PR.Pool.childrenOf(Idx)[0], "point");
}

TEST(ParserTestExpr, FieldAccessOnTupleIndex) {
  parseSource("fn main(){ let imm x = t.0.x; }");

  EXPECT_TRUE(PR.ok());

  // (t.0).x — the tuple index binds first, then the field access.
  const NodeIndex Field = PR.Pool.childrenOf(firstLet())[0];
  EXPECT_EQ(PR.Pool.kindOf(Field), NodeKind::FieldExpr);
  checkInternedString(Field, "x");
  EXPECT_TRUE(checkChildrenKinds(Field, NodeKind::TupleIndexExpr));
}

TEST(ParserTestExpr, TupleTypeInParam) {
  parseSource("fn dist(p: (i32, i32)): i32 {}");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Fn = PR.Pool.childrenOf(PR.Root)[0];
  const NodeIndex Params = PR.Pool.childrenOf(Fn)[0];
  const NodeIndex P = PR.Pool.childrenOf(Params)[0];
  EXPECT_TRUE(checkChildrenKinds(P, NodeKind::TupleType));
}

TEST(ParserTestExpr, TupleExprMissingClose) {
  parseSource("fn main(){ let imm t = (1, 2; }");

  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedTupleExprClose));
}

TEST(ParserTestExpr, TupleTypeMissingClose) {
  parseSource("fn main(){ let imm t: (i32, f64 = (); }");

  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedTupleTypeClose));
}

//===----------------------------------------------------------------------===//
// Field access
//===----------------------------------------------------------------------===//

TEST(ParserTestExpr, LetWithFieldAccess) {
  parseSource("fn main(){ let imm x: i32 = point.x; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::FieldExpr));

  const NodeIndex Field = PR.Pool.childrenOf(Let)[1];
  checkInternedString(Field, "x");
  checkSpan(Field, "point.x");
  EXPECT_TRUE(checkChildrenKinds(Field, NodeKind::IdentExpr));
  checkInternedString(PR.Pool.childrenOf(Field)[0], "point");
}

TEST(ParserTestExpr, LetWithNestedFieldAccess) {
  parseSource("fn main(){ let imm x: i32 = s.inner.value; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::FieldExpr));

  // Field access is left-associative: (s.inner).value
  const NodeIndex Outer = PR.Pool.childrenOf(Let)[1];
  checkInternedString(Outer, "value");
  checkSpan(Outer, "s.inner.value");
  EXPECT_TRUE(checkChildrenKinds(Outer, NodeKind::FieldExpr));

  const NodeIndex Inner = PR.Pool.childrenOf(Outer)[0];
  checkInternedString(Inner, "inner");
  checkSpan(Inner, "s.inner");
  EXPECT_TRUE(checkChildrenKinds(Inner, NodeKind::IdentExpr));
  checkInternedString(PR.Pool.childrenOf(Inner)[0], "s");
}

TEST(ParserTestExpr, FieldAccessOnCallResult) {
  parseSource("fn main(){ let imm x: i32 = make().x; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::FieldExpr));

  const NodeIndex Field = PR.Pool.childrenOf(Let)[1];
  checkInternedString(Field, "x");
  EXPECT_TRUE(checkChildrenKinds(Field, NodeKind::CallExpr));
}

TEST(ParserTestExpr, CallOnFieldAccess) {
  parseSource("fn main(){ let imm x: i32 = s.callback(1); }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::CallExpr));

  // The callee is the field access s.callback.
  const NodeIndex Call = PR.Pool.childrenOf(Let)[1];
  EXPECT_TRUE(checkChildrenKinds(Call, NodeKind::FieldExpr, NodeKind::ArgList));

  const NodeIndex Callee = PR.Pool.childrenOf(Call)[0];
  checkInternedString(Callee, "callback");
  EXPECT_TRUE(checkChildrenKinds(Callee, NodeKind::IdentExpr));
  checkInternedString(PR.Pool.childrenOf(Callee)[0], "s");
}

TEST(ParserTestExpr, FieldAccessOnIndexExpr) {
  parseSource("fn main(){ let imm x: i32 = items[0].x; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::FieldExpr));

  const NodeIndex Field = PR.Pool.childrenOf(Let)[1];
  checkInternedString(Field, "x");
  EXPECT_TRUE(checkChildrenKinds(Field, NodeKind::IndexExpr));
}

TEST(ParserTestExpr, FieldAccessBindsTighterThanBinary) {
  parseSource("fn main(){ let imm x: i32 = p.x + p.y; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Let = firstLet();
  EXPECT_TRUE(
      checkChildrenKinds(Let, NodeKind::NamedType, NodeKind::BinaryExpr));

  const NodeIndex Add = PR.Pool.childrenOf(Let)[1];
  EXPECT_TRUE(
      checkChildrenKinds(Add, NodeKind::FieldExpr, NodeKind::FieldExpr));
  checkInternedString(PR.Pool.childrenOf(Add)[0], "x");
  checkInternedString(PR.Pool.childrenOf(Add)[1], "y");
}

TEST(ParserTestExpr, AssignToField) {
  parseSource("fn main(){ p.x = 3; }");

  EXPECT_TRUE(PR.ok());

  const NodeIndex Body = fnBody();
  EXPECT_TRUE(checkChildrenKinds(Body, NodeKind::BinaryExpr));

  const NodeIndex Assign = PR.Pool.childrenOf(Body)[0];
  EXPECT_TRUE(
      checkChildrenKinds(Assign, NodeKind::FieldExpr, NodeKind::LitExpr));
  checkInternedString(PR.Pool.childrenOf(Assign)[0], "x");
  checkInternedString(PR.Pool.childrenOf(Assign)[1], "3");
}

//===----------------------------------------------------------------------===//
// Error recovery
//===----------------------------------------------------------------------===//

TEST(ParserTestExpr, StructLitBadFieldSingleError) {
  parseSource("fn main(){ let imm p = X { 42 }; }");

  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedFieldInitName));
  EXPECT_EQ(PR.Errors.size(), 1u);
}

TEST(ParserTestExpr, StructLitMissingCloseBrace) {
  parseSource("fn main(){ let imm p = X { x: 1 ; }");

  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedStructLitClose));
}

TEST(ParserTestExpr, LetBadInitSingleError) {
  // The bad initialiser must produce exactly one diagnostic: the missing ';'
  // and unclosed-block follow-ups are a cascade of the same mistake.
  parseSource("fn main() {\n    let imm x: i32 = + 1\n}");

  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
  EXPECT_EQ(PR.Errors.size(), 1u);
}

TEST(ParserTestExpr, FieldAccessMissingName) {
  parseSource("fn main(){ let imm x: i32 = p.; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedFieldName));
}

TEST(ParserTestExpr, ArithMissingRhs) {
  parseSource("fn main() { let imm x: i32 = 1 + ; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithMinusMissingRhs) {
  parseSource("fn main() { let imm x: i32 = 1 - ; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithModuloMissingRhs) {
  parseSource("fn main() { let imm x: i32 = 1 % ; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithChainedMissingRhs) {
  parseSource("fn main() { let imm x: i32 = 1 + 2 + ; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithDoubleOp) {
  parseSource("fn main() { let imm x: i32 = 1 + + 2; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithPlusNotPrefix) {
  parseSource("fn main() { let imm x: i32 = + 1; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithStarNotPrefix) {
  parseSource("fn main() { let imm x: i32 = * 1; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithMissingCloseParen) {
  parseSource("fn main() { let imm x: i32 = (1 + 2; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedParenExprClose));
}

TEST(ParserTestExpr, ArithUnaryMissingOperand) {
  parseSource("fn main() { let imm x: i32 = -; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithMissingRhsExprStmt) {
  parseSource("fn main() { 1 + ; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, ArithDoubleOpExprStmt) {
  parseSource("fn main() { 1 + + 2; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, CallMissingArg) {
  parseSource("fn main() { foo(1, ); }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedExpr));
}

TEST(ParserTestExpr, CallMissingCloseParen) {
  parseSource("fn main() { foo(1; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedArgListClose));
}

TEST(ParserTestExpr, IndexMissingCloseBracket) {
  parseSource("fn main() { arr[0; }");
  EXPECT_FALSE(PR.ok());
  EXPECT_TRUE(hasDiag(DiagID::ExpectedRSquare));
}
