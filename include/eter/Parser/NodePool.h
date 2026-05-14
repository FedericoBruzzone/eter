//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef ETER_PARSER_NODEPOOL_H
#define ETER_PARSER_NODEPOOL_H

#include "eter/Base/Span.h"
#include "eter/Base/StringInterner.h"
#include "eter/Parser/Regime.h"

#include <llvm/ADT/ArrayRef.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace eter::parser {

/// Identifies the syntactic role of an AST node.
enum class NodeKind : uint16_t {
#define ETER_NODE(X) X,
#include "eter/Parser/NodeKinds.def"
};

/// Return a human-readable name for a `NodeKind` (useful for diagnostics and
/// tests).
[[nodiscard]] const char *nodeKindName(NodeKind K);

/// An opaque handle to a node stored in a `NodePool`.
///
/// Prefer `NodeIndex` over raw pointers: it is 4 bytes instead of 8, never
/// dangling as long as the owning `NodePool` is alive, and trivially
/// serializable.
using NodeIndex = uint32_t;

/// Sentinel value meaning "no node" (analogous to a null pointer).
inline constexpr NodeIndex NullNode = std::numeric_limits<uint32_t>::max();

//===----------------------------------------------------------------------===//
// NodeData — 20-byte flat node record
//===----------------------------------------------------------------------===//

/// Raw record for a single AST node stored inside `NodePool`.
///
/// ## Payload encoding
///
/// `Payload` is a multipurpose 32-bit field whose meaning depends on `Kind`:
///
/// Nodes that carry a name AND an optional regime (FnDecl, LetStmt, Param,
/// StructField, EnumVariant*, IdentPat, ForStmt):
///     bits [31:30] = Regime  (0=Fix, 1=Mut, 2=Proj, 3=None)
///     bits [29: 0] = InternedStr (name, max ~1 billion unique strings)
///
/// Nodes that carry only a name (StructDecl, EnumDecl, ModDecl, ConstDecl,
/// UseDecl, IdentExpr, LitExpr, FieldExpr, StructLitExpr, FieldInit,
/// Attribute, NamedType, AttrType, LiteralPat, FieldPat):
///     bits [31: 0] = InternedStr (name or literal text)
///
/// Operator nodes (BinaryExpr, UnaryExpr):
///     bits [15: 0] = Token::Kind value (the operator)
///     bits [31:16] = 0
///
/// All other nodes: Payload = 0.
///
/// Always use `NodePool::makePayload`, `NodePool::payloadStr`, and
/// `NodePool::payloadRegime` instead of accessing `Payload` directly. This
/// enforces the encoding contract and makes the calling code self-documenting.
///
/// FIXME(bruzzone): We may want to remove the `Span` field from
/// `NodeData` and instead store all `Span`s in a separate "parallel" vector.
/// This would allow us to increase the data locality. In a 64-byte cache line,
/// we would have 5 `NodeData` instead of 3.
struct NodeData {
  NodeKind Kind;          ///< 2 bytes: syntactic role
  uint16_t ChildCount;    ///< 2 bytes: number of children
  uint32_t ChildrenBegin; ///< 4 bytes: start index into NodePool::Children
  uint32_t Payload;       ///< 4 bytes: see encoding above
  Span Span;              ///< 8 bytes: source byte range [Start, End).
                          //  20 bytes total: 3 nodes per 64-byte cache line
};

static_assert(sizeof(NodeData) == 20, "NodeData layout changed unexpectedly");

/// Owns all AST nodes and their child relationships for a single parse result.
///
/// ## Allocation model
/// All nodes are stored contiguously in `Nodes`. Children are stored as
/// contiguous index slices in the separate `Children` array. This layout
/// provides excellent cache locality when traversing a node's children:
/// iterating children is a sequential scan of a small slice, not a
/// pointer-chasing walk through the heap.
///
/// ## Lifetime and deallocation
/// `NodePool` is owned by value inside `ParseResult`. There are no individual
/// per-node `delete` calls: all memory is reclaimed at once when the owning
/// `ParseResult` is destroyed and the two `std::vector` members
/// (`Nodes`, `Children`) run their destructors. This is the same strategy
/// used by Clang (`ASTContext` + `BumpPtrAllocator`) and rustc (`TypedArena`):
/// bulk-free after a compiler phase completes, never node-by-node.
///
/// ## Invariants
///   - Index 0 is never allocated; `NullNode` (UINT32_MAX) means "absent".
///   - A node's `ChildrenBegin + ChildCount` must not exceed `Children.size()`.
///   - `NodeKind::Error` nodes may appear anywhere in the tree.
class NodePool {
public:
  NodePool();

  /// Allocate a new node with the given kind, source span, children, and
  /// payload. Returns the index of the newly created node.
  [[nodiscard]] NodeIndex alloc(NodeKind Kind, Span Span,
                                llvm::ArrayRef<NodeIndex> Kids,
                                uint32_t Payload = 0);

  /// Convenience: allocate a leaf node (no children).
  [[nodiscard]] NodeIndex allocLeaf(NodeKind Kind, Span Span,
                                    uint32_t Payload = 0);

  [[nodiscard]] const NodeData &operator[](NodeIndex I) const;
  [[nodiscard]] NodeKind kindOf(NodeIndex I) const;
  [[nodiscard]] Span spanOf(NodeIndex I) const;

  /// Return the contiguous slice of child indices for node `I`.
  [[nodiscard]] llvm::ArrayRef<NodeIndex> childrenOf(NodeIndex I) const;

  /// Return the `N`-th child of node `I`.
  /// Asserts in debug builds if `N` is out of bounds.
  [[nodiscard]] NodeIndex childAt(NodeIndex I, uint16_t N) const;

  /// Return the number of nodes currently allocated in the pool.
  [[nodiscard]] uint32_t size() const { return Nodes.size(); }

  /// Encode a name + regime into a Payload word.
  /// bits [31:30] = R, bits [29:0] = Name.
  [[nodiscard]] static uint32_t makePayload(InternedStr Name, Regime R);

  /// Extract the `InternedStr` from a Payload word (lower 30 bits).
  [[nodiscard]] static InternedStr payloadStr(uint32_t P);

  /// Extract the `Regime` from a Payload word (upper 2 bits).
  [[nodiscard]] static Regime payloadRegime(uint32_t P);

  /// Encode a `Token::Kind` value into a Payload word (operator nodes).
  [[nodiscard]] static uint32_t makeOpPayload(uint16_t TokenKindValue);

  /// Extract the `Token::Kind` value from an operator Payload word.
  [[nodiscard]] static uint16_t payloadOp(uint32_t P);

private:
  std::vector<NodeData> Nodes;     ///< all nodes; index 0 unused (sentinel)
  std::vector<NodeIndex> Children; ///< flat array of all children
};

} // namespace eter::parser

#endif // ETER_PARSER_NODEPOOL_H
