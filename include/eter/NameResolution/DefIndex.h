//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef ETER_NAMERESOLUTION_DEFINDEX_H
#define ETER_NAMERESOLUTION_DEFINDEX_H

#include "eter/Base/StringInterner.h"
#include "eter/Parser/NodePool.h"

#include <llvm/ADT/DenseMap.h>

#include <cstdint>
#include <limits>

namespace eter::name_resolution {

/// A stable identifier for a definition, assigned by its fully-qualified path
/// (e.g. `"pkg::utils::helper::x"`) rather than by its position in the AST.
///
/// `DefIndex` values are stable across incremental reparses: the same
/// declaration always receives the same `DefIndex` as long as its qualified
/// path does not change. This enables semantic passes (type checker, borrow
/// checker) to cache results keyed by `DefIndex` without a full query system —
/// cache invalidation only needs to touch the affected definitions.
///
/// ## Global vs. local definitions
/// Top-level items (`fn`, `struct`, `enum`, `mod`, `const`) receive a
/// `DefIndex` that is globally unique within a pack. Local definitions
/// (`let` bindings, function parameters) receive a `DefIndex` that is unique
/// within their enclosing function scope — analogous to rustc's `LocalDefId`.
/// Both are represented by the same `DefIndex` type; the distinction is
/// enforced by the `DefIndexBuilder` during name resolution.
struct DefIndex {
  uint32_t Value = std::numeric_limits<uint32_t>::max();

  [[nodiscard]] bool isValid() const {
    return Value != std::numeric_limits<uint32_t>::max();
  }
  [[nodiscard]] bool operator==(DefIndex Other) const {
    return Value == Other.Value;
  }
  [[nodiscard]] bool operator!=(DefIndex Other) const {
    return Value != Other.Value;
  }
};

/// Sentinel: an unassigned or invalid definition index.
inline constexpr DefIndex InvalidDef{};

/// Assigns `DefIndex` values to every definition in an AST.
///
/// This is a name-resolution pass that runs after parsing is complete. It
/// walks the full AST recursively — not just the top-level declarations — so
/// that local definitions (`let` bindings, parameters) also receive stable
/// `DefIndex` values for use by subsequent semantic passes.
///
/// ## Scope rules
/// - Top-level items are keyed by their fully-qualified path
///   (e.g. `"mod::sub::fn_name"`).
/// - Local definitions (inside function bodies) are keyed by their enclosing
///   function's path plus a disambiguating counter
///   (e.g. `"mod::fn_name::x#0"`).
///
/// ## Usage
/// One `DefIndexBuilder` per pack (not per file), constructed with the
/// pack's shared `StringInterner`. Call `build` once per `ParseResult`
/// after all files in the pack have been parsed.
class DefIndexBuilder {
public:
  explicit DefIndexBuilder(parser::StringInterner &Interner);

  /// Walk the full AST rooted at `Root` in `Pool` and assign a `DefIndex`
  /// to every definition node. Returns a mapping from NodeIndex → DefIndex.
  [[nodiscard]] llvm::DenseMap<parser::NodeIndex, DefIndex>
  build(const parser::NodePool &Pool, parser::NodeIndex Root);

private:
  [[maybe_unused]] parser::StringInterner &Interner;
  llvm::DenseMap<parser::InternedStr, DefIndex> PathToIndex;
  [[maybe_unused]] uint32_t NextId = 0;

  DefIndex getOrAssign(parser::InternedStr QualifiedPath);
};

} // namespace eter::name_resolution

#endif // ETER_NAMERESOLUTION_DEFINDEX_H
