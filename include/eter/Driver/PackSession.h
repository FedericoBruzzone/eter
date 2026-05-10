//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef ETER_DRIVER_PACKSESSION_H
#define ETER_DRIVER_PACKSESSION_H

#include "eter/Base/StringInterner.h"
#include "eter/Parser/Parser.h"

#include <llvm/ADT/StringMap.h>

namespace eter {

/// Holds the shared state for a single `eterc` invocation over one pack
/// (a tree of source files connected by `mod foo;` declarations).
///
/// ## Ownership
/// `PackSession` is owned by the Driver for the lifetime of a single
/// compilation. It outlives all `ParseResult`s it contains, ensuring that
/// `InternedStr` IDs remain valid for cross-file comparisons throughout
/// every compiler pass.
///
/// ## Pack model
/// A pack is the Eter compilation unit: one root `.et` file plus all files
/// transitively reachable via `mod foo;` declarations. `eterc` receives only
/// the root file path, no manifest. Future works may add a separate build tool,
// the Courier, that reads `Eter.toml` and invokes `eterc` with the appropriate
/// root.
///
/// ## StringInterner sharing
/// All files in a pack share the same `StringInterner`. This guarantees that
/// interning the same identifier string in two different files produces the
/// same `InternedStr` ID, enabling O(1) cross-file name comparison in every
/// subsequent semantic pass.
struct PackSession {
  /// String interner shared across all source files in the pack.
  parser::StringInterner Interner;

  /// Parse results keyed by canonical file path, one entry per source file.
  /// Populated incrementally by the Driver as files are discovered via
  /// `mod foo;` declarations.
  llvm::StringMap<parser::ParseResult> Results;
};

} // namespace eter

#endif // ETER_DRIVER_PACKSESSION_H
