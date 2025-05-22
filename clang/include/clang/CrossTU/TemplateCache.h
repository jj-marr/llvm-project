//===--- TemplateCache.h - Template Caching for CTU -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file provides template caching functionality for Cross-Translation
//  Unit analysis. It extends the existing CTU infrastructure to cache and
//  reuse template instantiations across translation units.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_CROSSTU_TEMPLATECACHE_H
#define LLVM_CLANG_CROSSTU_TEMPLATECACHE_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/LLVM.h"
#include "clang/CrossTU/CrossTranslationUnit.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Error.h"
#include <chrono>
#include <memory>
#include <optional>

namespace clang {
class ASTUnit;
class CompilerInstance;
class ConceptDecl;
class Decl;
class TemplateArgumentList;
class TemplateDecl;

namespace cross_tu {

/// Template-specific error codes extending the existing index_error_code
enum class template_cache_error_code {
  success = 0,
  unspecified = 1,
  invalid_template_usr,
  template_instantiation_failed,
  template_cache_corrupted,
  template_argument_mismatch,
  constraint_evaluation_failed,
  template_not_found_in_cache,
  template_cache_write_failed,
  template_dependency_changed
};

/// Error class for template caching operations
class TemplateCacheError : public llvm::ErrorInfo<TemplateCacheError> {
public:
  static char ID;
  TemplateCacheError(template_cache_error_code C) : Code(C) {}
  TemplateCacheError(template_cache_error_code C, std::string Message)
      : Code(C), Message(std::move(Message)) {}

  void log(raw_ostream &OS) const override;
  std::error_code convertToErrorCode() const override;
  template_cache_error_code getCode() const { return Code; }
  const std::string &getMessage() const { return Message; }

private:
  template_cache_error_code Code;
  std::string Message;
};

/// Unique identifier for template instantiations
struct TemplateIdentifier {
  std::string TemplateUSR;           // Base template USR
  std::string CanonicalArguments;    // Canonicalized template arguments
  std::string InstantiationContext;  // Nested template context
  TemplateSpecializationKind Kind;   // Explicit/implicit specialization

  TemplateIdentifier() = default;
  TemplateIdentifier(StringRef TUSR, StringRef Args, StringRef Context,
                    TemplateSpecializationKind K)
      : TemplateUSR(TUSR), CanonicalArguments(Args),
        InstantiationContext(Context), Kind(K) {}

  bool operator==(const TemplateIdentifier &Other) const {
    return TemplateUSR == Other.TemplateUSR &&
           CanonicalArguments == Other.CanonicalArguments &&
           InstantiationContext == Other.InstantiationContext &&
           Kind == Other.Kind;
  }

  bool operator<(const TemplateIdentifier &Other) const {
    if (TemplateUSR != Other.TemplateUSR)
      return TemplateUSR < Other.TemplateUSR;
    if (CanonicalArguments != Other.CanonicalArguments)
      return CanonicalArguments < Other.CanonicalArguments;
    if (InstantiationContext != Other.InstantiationContext)
      return InstantiationContext < Other.InstantiationContext;
    return Kind < Other.Kind;
  }

  std::string toString() const;
};

/// Metadata for cached template instantiations
struct TemplateInstantiationInfo {
  SourceLocation PointOfInstantiation;
  TemplateSpecializationKind SpecKind;
  std::string SourceFile;
  std::string CacheFile;
  bool IsConstraintSatisfied;
  std::chrono::time_point<std::chrono::system_clock> CacheTime;
  std::vector<std::string> DependentHeaders;

  TemplateInstantiationInfo() = default;
  TemplateInstantiationInfo(SourceLocation POI, TemplateSpecializationKind SK,
                           StringRef Source, StringRef Cache, bool Satisfied)
      : PointOfInstantiation(POI), SpecKind(SK), SourceFile(Source),
        CacheFile(Cache), IsConstraintSatisfied(Satisfied),
        CacheTime(std::chrono::system_clock::now()) {}

  bool isValid() const;
};

/// Generates USRs for template instantiations
class TemplateUSRGenerator {
public:
  TemplateUSRGenerator(ASTContext &Ctx) : Context(Ctx) {}

  /// Generate a USR for a class template specialization
  llvm::Expected<std::string> generateUSR(
      const ClassTemplateSpecializationDecl *Spec);

  /// Generate a USR for a function template specialization
  llvm::Expected<std::string> generateUSR(
      const FunctionDecl *FD,
      const FunctionTemplateSpecializationInfo *Spec);

  /// Generate a USR for a variable template specialization
  llvm::Expected<std::string> generateUSR(
      const VarTemplateSpecializationDecl *Spec);

  /// Generate a template identifier from template arguments
  llvm::Expected<TemplateIdentifier> generateTemplateIdentifier(
      const TemplateDecl *Template,
      const TemplateArgumentList &Args,
      TemplateSpecializationKind Kind);

private:
  ASTContext &Context;

  /// Canonicalize template arguments for consistent USR generation
  std::string canonicalizeTemplateArguments(const TemplateArgumentList &Args);

  /// Get the instantiation context for nested templates
  std::string getInstantiationContext(const Decl *D);
};

/// Storage for template instantiation cache extending ASTUnitStorage pattern
class TemplateASTUnitStorage {
public:
  TemplateASTUnitStorage(CompilerInstance &CI);

  /// Get an ASTUnit for a template instantiation
  llvm::Expected<ASTUnit *> getASTUnitForTemplate(
      const TemplateIdentifier &TID,
      StringRef CrossTUDir,
      StringRef IndexName,
      bool DisplayProgress = false);

  /// Get a cached template instantiation
  llvm::Expected<const Decl *> getCachedTemplateInstantiation(
      const TemplateIdentifier &TID);

  /// Cache a template instantiation
  llvm::Error cacheTemplateInstantiation(
      const TemplateIdentifier &TID,
      const Decl *InstantiatedDecl,
      ASTUnit *SourceUnit);

  /// Get the file path for a template instantiation
  llvm::Expected<std::string> getFileForTemplate(
      const TemplateIdentifier &TID,
      StringRef CrossTUDir,
      StringRef IndexName);

private:
  llvm::Error ensureTemplateIndexLoaded(StringRef CrossTUDir,
                                       StringRef IndexName);
  llvm::Expected<ASTUnit *> getASTUnitForFile(StringRef FileName,
                                             bool DisplayProgress);

  using TemplateFileMapTy = llvm::StringMap<std::unique_ptr<ASTUnit>>;
  using TemplateNameMapTy = llvm::StringMap<ASTUnit *>;
  using TemplateIndexMapTy = llvm::StringMap<std::string>;
  using TemplateSpecMapTy = llvm::DenseMap<TemplateIdentifier,
                                         TemplateInstantiationInfo>;

  TemplateFileMapTy TemplateFileASTUnitMap;
  TemplateNameMapTy TemplateNameASTUnitMap;
  TemplateIndexMapTy TemplateNameFileMap;
  TemplateSpecMapTy TemplateInstantiationSpecMap;

  CompilerInstance &CI;
  std::unique_ptr<class TemplateASTLoader> Loader;

public:
  ~TemplateASTUnitStorage();
};

/// Main interface for template caching operations
class TemplateInstantiationCache {
public:
  TemplateInstantiationCache(CompilerInstance &CI,
                           CrossTranslationUnitContext &CTU);

  /// Try to get a cached template instantiation
  llvm::Expected<const Decl *> getCachedTemplateInstantiation(
      const ClassTemplateSpecializationDecl *Spec,
      StringRef CrossTUDir,
      StringRef IndexName);

  llvm::Expected<const Decl *> getCachedTemplateInstantiation(
      const FunctionDecl *FD,
      const FunctionTemplateSpecializationInfo *Spec,
      StringRef CrossTUDir,
      StringRef IndexName);

  llvm::Expected<const Decl *> getCachedTemplateInstantiation(
      const VarTemplateSpecializationDecl *Spec,
      StringRef CrossTUDir,
      StringRef IndexName);

  /// Cache a template instantiation
  llvm::Error cacheTemplateInstantiation(
      const Decl *InstantiatedDecl,
      const TemplateArgumentList &Args,
      StringRef CrossTUDir,
      StringRef IndexName);

  /// Get cached constraint satisfaction result
  llvm::Expected<bool> getCachedConstraintSatisfaction(
      const ConceptDecl *Concept,
      const TemplateArgumentList &Args,
      StringRef CrossTUDir,
      StringRef IndexName);

  /// Cache constraint satisfaction result
  llvm::Error cacheConstraintSatisfaction(
      const ConceptDecl *Concept,
      const TemplateArgumentList &Args,
      bool IsSatisfied,
      StringRef CrossTUDir,
      StringRef IndexName);

  /// Check if a template instantiation is cached
  bool isTemplateCached(const TemplateIdentifier &TID,
                       StringRef CrossTUDir,
                       StringRef IndexName);

  /// Invalidate cache entries based on dependency changes
  llvm::Error invalidateDependentCaches(StringRef HeaderPath,
                                       StringRef CrossTUDir,
                                       StringRef IndexName);

private:
  CompilerInstance &CI;
  CrossTranslationUnitContext &CTUContext;
  std::unique_ptr<TemplateUSRGenerator> USRGen;
  std::unique_ptr<TemplateASTUnitStorage> Storage;

  /// Helper to generate template identifier from various template types
  llvm::Expected<TemplateIdentifier> getTemplateIdentifier(const Decl *D);

public:
  ~TemplateInstantiationCache();
};

/// Parse template cache index file
llvm::Expected<llvm::StringMap<std::string>>
parseTemplateCacheIndex(StringRef IndexPath);

/// Create template cache index string
std::string createTemplateCacheIndexString(
    const llvm::StringMap<std::string> &Index);

} // namespace cross_tu
} // namespace clang

/// Hash function for TemplateIdentifier to use in DenseMap
namespace llvm {
template <>
struct DenseMapInfo<clang::cross_tu::TemplateIdentifier> {
  static clang::cross_tu::TemplateIdentifier getEmptyKey() {
    return clang::cross_tu::TemplateIdentifier("<<empty>>", "", "",
                                              clang::TSK_Undeclared);
  }

  static clang::cross_tu::TemplateIdentifier getTombstoneKey() {
    return clang::cross_tu::TemplateIdentifier("<<tombstone>>", "", "",
                                              clang::TSK_Undeclared);
  }

  static unsigned getHashValue(const clang::cross_tu::TemplateIdentifier &TID) {
    return hash_combine(hash_value(TID.TemplateUSR),
                       hash_value(TID.CanonicalArguments),
                       hash_value(TID.InstantiationContext),
                       hash_value(static_cast<int>(TID.Kind)));
  }

  static bool isEqual(const clang::cross_tu::TemplateIdentifier &LHS,
                     const clang::cross_tu::TemplateIdentifier &RHS) {
    return LHS == RHS;
  }
};
} // namespace llvm

#endif // LLVM_CLANG_CROSSTU_TEMPLATECACHE_H