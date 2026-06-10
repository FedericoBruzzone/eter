//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "eter/Base/Debug.h"
#include "eter/Base/Span.h"
#include "eter/Base/StringInterner.h"
#include "eter/Parser/NodePool.h"
#include "eter/Parser/Parser.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG_TYPE "parser-decl"

namespace eter::parser {

NodeIndex Parser::parseTopLevelDecl(llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseTopLevelDecl\n");
  using Kind = lexer::Token::Kind;

  const llvm::SmallVector<NodeIndex, 4> Docs = parseDocComments();

  switch (peek()) {
  case Kind::kw_fn:
    return parseFnDecl(Docs, Attrs);
  case Kind::kw_const:
    return parseConstDecl(Docs, Attrs);
  case Kind::kw_mod:
    return parseModDecl(Docs, Attrs);
  case Kind::kw_struct:
    return parseStructDecl(Docs, Attrs);
  case Kind::kw_enum:
    return parseEnumDecl(Docs, Attrs);
  case Kind::kw_union:
    return parseUnionDecl(Docs, Attrs);
  case Kind::kw_use:
    return parseUseDecl(Docs, Attrs);
  default: {
    const lexer::Token Tok = peekToken();
    addError(Tok.TokenSpan, DiagID::ExpectedTopLevelDecl);
    synchronize();
    return makeErrorNode(Tok.TokenSpan);
  }
  }
}

NodeIndex Parser::parseFnDecl(llvm::ArrayRef<NodeIndex> Docs,
                              llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseFnDecl\n");
  using Kind = lexer::Token::Kind;

  const Span Start = expect(Kind::kw_fn, DiagID::ExpectedFnKeyword).TokenSpan;

  const Regime ReturnRegime = parseRegime();

  const InternedStr Name =
      expectAndIntern(Kind::identifier, DiagID::ExpectedFnName);

  llvm::SmallVector<NodeIndex, 8> Children(Docs.begin(), Docs.end());
  Children.append(Attrs.begin(), Attrs.end());
  Children.push_back(parseParamList());

  if (consume(Kind::colon))
    Children.push_back(parseType());

  Children.push_back(parseBlockExpr());

  return Pool.alloc(NodeKind::FnDecl,
                    Span{Start.Start, Stream.previous().TokenSpan.End},
                    Children, NodePool::makePayload(Name, ReturnRegime));
}

NodeIndex Parser::parseStructDecl(llvm::ArrayRef<NodeIndex> Docs,
                                  llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseStructDecl\n");
  using Kind = lexer::Token::Kind;

  const Span Start =
      expect(Kind::kw_struct, DiagID::ExpectedStructKeyword).TokenSpan;

  const InternedStr Name =
      expectAndIntern(Kind::identifier, DiagID::ExpectedStructName);

  expect(Kind::l_brace, DiagID::ExpectedStructOpen);

  llvm::SmallVector<NodeIndex, 8> Children(Docs.begin(), Docs.end());
  Children.append(Attrs.begin(), Attrs.end());
  parseCommaSeparated(Children, Kind::r_brace,
                      [this] { return parseStructField(); });

  const Span End = expect(Kind::r_brace, DiagID::ExpectedStructClose).TokenSpan;
  return Pool.alloc(NodeKind::StructDecl, Span{Start.Start, End.End}, Children,
                    Name);
}

NodeIndex Parser::parseEnumDecl(llvm::ArrayRef<NodeIndex> Docs,
                                llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseEnumDecl\n");
  using Kind = lexer::Token::Kind;

  const Span Start =
      expect(Kind::kw_enum, DiagID::ExpectedEnumKeyword).TokenSpan;

  const InternedStr Name =
      expectAndIntern(Kind::identifier, DiagID::ExpectedEnumName);

  expect(Kind::l_brace, DiagID::ExpectedEnumOpen);

  llvm::SmallVector<NodeIndex, 8> Children(Docs.begin(), Docs.end());
  Children.append(Attrs.begin(), Attrs.end());
  parseCommaSeparated(Children, Kind::r_brace,
                      [this] { return parseEnumVariant(); });

  const Span End = expect(Kind::r_brace, DiagID::ExpectedEnumClose).TokenSpan;
  return Pool.alloc(NodeKind::EnumDecl, Span{Start.Start, End.End}, Children,
                    Name);
}

NodeIndex Parser::parseUnionDecl(llvm::ArrayRef<NodeIndex> Docs,
                                 llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseUnionDecl\n");
  using Kind = lexer::Token::Kind;

  const Span Start =
      expect(Kind::kw_union, DiagID::ExpectedUnionKeyword).TokenSpan;

  const InternedStr Name =
      expectAndIntern(Kind::identifier, DiagID::ExpectedUnionName);

  expect(Kind::l_brace, DiagID::ExpectedUnionOpen);

  llvm::SmallVector<NodeIndex, 8> Children(Docs.begin(), Docs.end());
  Children.append(Attrs.begin(), Attrs.end());
  parseCommaSeparated(Children, Kind::r_brace,
                      [this] { return parseStructField(); });

  const Span End = expect(Kind::r_brace, DiagID::ExpectedUnionClose).TokenSpan;
  return Pool.alloc(NodeKind::UnionDecl, Span{Start.Start, End.End}, Children,
                    Name);
}

NodeIndex Parser::parseModDecl(llvm::ArrayRef<NodeIndex> Docs,
                               llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseModDecl\n");
  using Kind = lexer::Token::Kind;

  const Span Start = expect(Kind::kw_mod, DiagID::ExpectedModKeyword).TokenSpan;

  const InternedStr Name =
      expectAndIntern(Kind::identifier, DiagID::ExpectedModName);

  if (consume(Kind::l_brace)) {
    // Inline module: mod name { TopLevelDecl* }
    llvm::SmallVector<NodeIndex, 8> Children(Docs.begin(), Docs.end());
    Children.append(Attrs.begin(), Attrs.end());
    while (!check(Kind::r_brace) && !atEof())
      Children.push_back(parseTopLevelDecl({}));
    const Span End = expect(Kind::r_brace, DiagID::ExpectedModClose).TokenSpan;
    return Pool.alloc(NodeKind::ModDecl, Span{Start.Start, End.End}, Children,
                      Name);
  }

  if (check(Kind::semi)) {
    const Span End = peekToken().TokenSpan;
    advance(); // consume ';'
    llvm::SmallVector<NodeIndex, 4> Children(Docs.begin(), Docs.end());
    Children.append(Attrs.begin(), Attrs.end());
    return Pool.alloc(NodeKind::ModDeclFile, Span{Start.Start, End.End},
                      Children, Name);
  }

  addError(peekToken().TokenSpan, DiagID::ExpectedModOpenOrSemi);
  return makeErrorNode(peekToken().TokenSpan);
}

NodeIndex
Parser::parseUseDecl([[maybe_unused]] llvm::ArrayRef<NodeIndex> Docs,
                     [[maybe_unused]] llvm::ArrayRef<NodeIndex> Attrs) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseUseDecl\n");
  llvm::report_fatal_error("TODO: implement Parser::parseUseDecl");
}

NodeIndex Parser::parseParamList() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseParamList\n");
  using Kind = lexer::Token::Kind;

  const Span Start =
      expect(Kind::l_paren, DiagID::ExpectedParamListOpen).TokenSpan;

  llvm::SmallVector<NodeIndex, 8> Children;
  if (!check(Kind::r_paren)) {
    Children.push_back(parseParam());
    while (consume(Kind::comma))
      Children.push_back(parseParam());
  }

  const Span End =
      expect(Kind::r_paren, DiagID::ExpectedParamListClose).TokenSpan;
  return Pool.alloc(NodeKind::ParamList, Span{Start.Start, End.End}, Children);
}

NodeIndex Parser::parseParam() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseParam\n");
  using Kind = lexer::Token::Kind;

  const uint32_t StartPos = peekToken().TokenSpan.Start;

  const Regime R = parseRegime();

  const InternedStr Name =
      expectAndIntern(Kind::identifier, DiagID::ExpectedParamName);

  expect(Kind::colon, DiagID::ExpectedParamColon);

  const NodeIndex Type = parseType();

  const NodeIndex Children[] = {Type};
  return Pool.alloc(NodeKind::Param,
                    Span{StartPos, Stream.previous().TokenSpan.End}, Children,
                    NodePool::makePayload(Name, R));
}

NodeIndex Parser::parseStructField() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseStructField\n");
  using Kind = lexer::Token::Kind;

  const uint32_t StartPos = peekToken().TokenSpan.Start;

  const Regime R = parseRegime();

  // Bail out at the first error: chaining the follow-up ':' and type
  // diagnostics for a field that never started would only repeat errors on
  // the same token. parseCommaSeparated resynchronises to the next field.
  if (!check(Kind::identifier)) {
    const Span S = peekToken().TokenSpan;
    addError(S, DiagID::ExpectedStructFieldName);
    return makeErrorNode(Span{StartPos, S.End});
  }
  const InternedStr Name = Interner.intern(textOf(advance().TokenSpan));

  if (!consume(Kind::colon)) {
    addError(peekToken().TokenSpan, DiagID::ExpectedStructFieldColon);
    // Only attempt the type when the next token could actually start one;
    // otherwise the missing ':' diagnostic already covers this field.
    if (!check(Kind::identifier))
      return makeErrorNode(Span{StartPos, Stream.previous().TokenSpan.End});
  }

  const NodeIndex Type = parseType();

  const NodeIndex Children[] = {Type};
  return Pool.alloc(NodeKind::StructField,
                    Span{StartPos, Stream.previous().TokenSpan.End}, Children,
                    NodePool::makePayload(Name, R));
}

NodeIndex Parser::parseEnumVariant() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseEnumVariant\n");
  using Kind = lexer::Token::Kind;

  const uint32_t StartPos = peekToken().TokenSpan.Start;

  // Bail out at the first error; parseCommaSeparated resynchronises to the
  // next variant, so a single bad variant yields a single diagnostic.
  if (!check(Kind::identifier)) {
    const Span S = peekToken().TokenSpan;
    addError(S, DiagID::ExpectedEnumVariantName);
    return makeErrorNode(S);
  }
  const InternedStr Name = Interner.intern(textOf(advance().TokenSpan));

  // Tuple variant: VariantName(Type*)
  if (consume(Kind::l_paren)) {
    llvm::SmallVector<NodeIndex, 4> Types;
    parseCommaSeparated(Types, Kind::r_paren, [this] { return parseType(); });
    const Span End =
        expect(Kind::r_paren, DiagID::ExpectedVariantTupleClose).TokenSpan;
    return Pool.alloc(NodeKind::EnumVariantTuple, Span{StartPos, End.End},
                      Types, NodePool::makePayload(Name, Regime::None));
  }

  // Struct variant: VariantName { StructField* }
  if (consume(Kind::l_brace)) {
    llvm::SmallVector<NodeIndex, 8> Fields;
    parseCommaSeparated(Fields, Kind::r_brace,
                        [this] { return parseStructField(); });
    const Span End =
        expect(Kind::r_brace, DiagID::ExpectedVariantStructClose).TokenSpan;
    return Pool.alloc(NodeKind::EnumVariantStruct, Span{StartPos, End.End},
                      Fields, NodePool::makePayload(Name, Regime::None));
  }

  // Unit variant with an explicit discriminant: VariantName = ConstExpr
  if (consume(Kind::eq)) {
    const NodeIndex Discriminant = parseConstExpr();
    const NodeIndex Children[] = {Discriminant};
    return Pool.alloc(NodeKind::EnumVariantUnit,
                      Span{StartPos, Stream.previous().TokenSpan.End}, Children,
                      NodePool::makePayload(Name, Regime::None));
  }

  // Unit variant: VariantName
  return Pool.allocLeaf(NodeKind::EnumVariantUnit,
                        Span{StartPos, Stream.previous().TokenSpan.End},
                        NodePool::makePayload(Name, Regime::None));
}

} // namespace eter::parser
