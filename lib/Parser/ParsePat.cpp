//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "eter/Base/Debug.h"
#include "eter/Parser/Parser.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG_TYPE "parser-pat"

namespace eter::parser {

NodeIndex Parser::parsePat() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parsePat\n");

  using Kind = lexer::Token::Kind;

  const lexer::Token Tok = Stream.peekToken();

  switch (Tok.TokenKind) {
  case Kind::identifier: {
    const llvm::StringRef Text = textOf(Tok.TokenSpan);
    if (Text == "_") {
      advance();
      return Pool.allocLeaf(NodeKind::WildcardPat, Tok.TokenSpan);
    }
    advance();
    // Possibly a variant path: Name :: Name (e.g. Animals::Cat).
    Span PathSpan = Tok.TokenSpan;
    parsePathSegments(PathSpan);
    const InternedStr Name = Interner.intern(textOf(PathSpan));

    if (check(Kind::l_paren))
      return parseTuplePat(Name, PathSpan);
    if (check(Kind::l_brace))
      return parseStructPat(Name, PathSpan);

    // A plain name binds the scrutinee; a `::`-qualified name refers to a
    // unit variant. Both are IdentPat — name resolution distinguishes them.
    return Pool.allocLeaf(NodeKind::IdentPat, PathSpan,
                          NodePool::makePayload(Name, Regime::None));
  }
  case Kind::l_paren:
    // Anonymous tuple pattern: ( Pat, ... )
    return parseTuplePat(NullStr, Tok.TokenSpan);
  case Kind::integer_literal:
  case Kind::float_literal:
  case Kind::char_literal:
  case Kind::string_literal:
  case Kind::kw_true:
  case Kind::kw_false: {
    advance();
    return Pool.allocLeaf(NodeKind::LiteralPat, Tok.TokenSpan,
                          Interner.intern(textOf(Tok.TokenSpan)));
  }
  default:
    addError(Tok.TokenSpan, DiagID::ExpectedPattern);
    advance();
    return makeErrorNode(Tok.TokenSpan);
  }
}

NodeIndex Parser::parseStructPat(InternedStr Name, Span Start) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseStructPat\n");
  using Kind = lexer::Token::Kind;

  advance(); // consume '{' (guaranteed by the caller's check)

  llvm::SmallVector<NodeIndex, 8> Fields;
  parseCommaSeparated(Fields, Kind::r_brace,
                      [this] { return parseFieldPat(); });

  const Span End =
      expect(Kind::r_brace, DiagID::ExpectedStructPatClose).TokenSpan;
  return Pool.alloc(NodeKind::StructPat, Span{Start.Start, End.End}, Fields,
                    Name);
}

NodeIndex Parser::parseTuplePat(InternedStr Name, Span Start) {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseTuplePat\n");
  using Kind = lexer::Token::Kind;

  advance(); // consume '(' (guaranteed by the caller's check)

  llvm::SmallVector<NodeIndex, 4> Pats;
  parseCommaSeparated(Pats, Kind::r_paren, [this] { return parsePat(); });

  const Span End =
      expect(Kind::r_paren, DiagID::ExpectedTuplePatClose).TokenSpan;
  return Pool.alloc(NodeKind::TuplePat, Span{Start.Start, End.End}, Pats, Name);
}

NodeIndex Parser::parseFieldPat() {
  ETER_DEBUG(llvm::dbgs() << "[" DEBUG_TYPE "] parseFieldPat\n");
  using Kind = lexer::Token::Kind;

  // Bail out at the first error; parseCommaSeparated resynchronises to the
  // next field.
  if (!check(Kind::identifier)) {
    const Span S = peekToken().TokenSpan;
    addError(S, DiagID::ExpectedFieldPatName);
    return makeErrorNode(S);
  }
  const lexer::Token NameTok = advance();
  const InternedStr Name = Interner.intern(textOf(NameTok.TokenSpan));

  // name : Pat — match the field against a nested pattern.
  if (consume(Kind::colon)) {
    const NodeIndex Pat = parsePat();
    return Pool.alloc(NodeKind::FieldPat,
                      Span{NameTok.TokenSpan.Start, Pool.spanOf(Pat).End},
                      {Pat}, Name);
  }

  // Shorthand `name` — bind the field to a local of the same name.
  return Pool.allocLeaf(NodeKind::FieldPat, NameTok.TokenSpan, Name);
}

} // namespace eter::parser
