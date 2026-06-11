//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "eter/Base/Debug.h"
#include "eter/Base/StringInterner.h"
#include "eter/Lexer/Token.h"
#include "eter/Parser/NodePool.h"
#include "eter/Parser/Parser.h"

#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG_TYPE "parser-type"

namespace eter::parser {

// FIXME First check for primitive types, then custom types
NodeIndex Parser::parseType() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseType\n");
  using Kind = lexer::Token::Kind;

  // Tuple type: ( Type, Type, ... ); `()` is the unit type.
  if (check(Kind::l_paren)) {
    const Span Start = advance().TokenSpan;
    llvm::SmallVector<NodeIndex, 4> Types;
    parseCommaSeparated(Types, Kind::r_paren, [this] { return parseType(); });
    const Span End =
        expect(Kind::r_paren, DiagID::ExpectedTupleTypeClose).TokenSpan;
    return Pool.alloc(NodeKind::TupleType, Span{Start.Start, End.End}, Types);
  }

  if (check(Kind::l_square))
    return parseArrayType();

  return parseNamedType();
}

NodeIndex Parser::parseNamedType() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseNamedType\n");
  using Kind = lexer::Token::Kind;

  Span Full = peekToken().TokenSpan;
  expect(Kind::identifier, DiagID::ExpectedTypeName);

  // Qualified name: name :: name (:: name)*  (e.g. math::Vec).
  parsePathSegments(Full);

  return Pool.allocLeaf(NodeKind::NamedType, Full,
                        Interner.intern(textOf(Full)));
}

NodeIndex Parser::parseArrayType() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseArrayType\n");
  using Kind = lexer::Token::Kind;

  const Span Start = advance().TokenSpan; // consume '[' (cf. parseType)

  llvm::SmallVector<NodeIndex, 4> Children;
  Children.push_back(parseType());

  expect(Kind::semi, DiagID::ExpectedArrayTypeSemi);

  // One size per dimension: [i32; 3] is an array, [f32; 2, 2] a tensor.
  parseCommaSeparated(Children, Kind::r_square,
                      [this] { return parseConstExpr(); });
  if (Children.size() == 1)
    addError(peekToken().TokenSpan, DiagID::ExpectedArraySize);

  const Span End =
      expect(Kind::r_square, DiagID::ExpectedArrayTypeClose).TokenSpan;
  return Pool.alloc(NodeKind::ArrayType, Span{Start.Start, End.End}, Children);
}

} // namespace eter::parser
