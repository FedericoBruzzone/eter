//===----------------------------------------------------------------------===//
//
// Part of the Eter Project, under the Apache License v2.0 with LLVM Exceptions.
// See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef ETER_DRIVER_DRIVER_H
#define ETER_DRIVER_DRIVER_H

#include <llvm/ADT/StringSet.h>

#include <string>
#include <vector>

namespace eter {

/// Represents command-line options for the Eter compiler.
struct CompilerOptions {
  std::vector<std::string> InputFiles;
  std::string OutputFile;
  bool ShowVersion = false;
  bool ShowHelp = false;
  int OptimizationLevel = 0; // -O0
};

/// Main compiler driver that orchestrates the compilation pipeline.
class Driver {
public:
  Driver();

  /// Parse command-line arguments.
  /// \returns true if parsing was successful, false otherwise.
  bool parseCommandLine(int Argc, char **Argv);

  /// Run the compilation pipeline with the parsed options.
  /// \returns 0 on success, non-zero error code on failure.
  int run();

  /// Get the parsed compiler options.
  const CompilerOptions &getOptions() const { return Options; }

  /// Print help message.
  void printHelp() const;

  /// Print version information.
  void printVersion() const;

private:
  /// Compile a complete pack rooted at `RootFile`.
  /// Discovers all reachable source files via `mod foo;` declarations,
  /// parses them with a shared `StringInterner`, and stores the results in
  /// a `PackSession`.
  /// \returns 0 on success, non-zero error code on failure.
  int compilePack(const std::string &RootFile);

  /// Parse one source file and, recursively, all files it references via
  /// `mod foo;`. Detects circular dependencies via `InProgress`.
  /// \param Path       Canonical path of the file to parse.
  /// \param Session    The shared pack session (interner + results).
  /// \param InProgress Set of paths currently being parsed (cycle detection).
  /// \returns 0 on success, non-zero on I/O or fatal error.
  int parseFile(const std::string &Path, struct PackSession &Session,
                llvm::StringSet<> &InProgress);

  CompilerOptions Options;
};

} // namespace eter

#endif // ETER_DRIVER_DRIVER_H
