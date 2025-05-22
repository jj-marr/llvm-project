//===--- TemplateInstantiationCache.h - Sema Template Caching -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file provides Sema-specific template caching interfaces that integrate
//  with Clang's semantic analysis phase to automatically intercept and cache
//  template instantiations during compilation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_TEMPLATEINSTANTIATIONCACHE_H
#define LLVM_CLANG_SEMA_TEMPLATEINSTANTIATIONCACHE_H

#include "clang/AST/DeclTemplate.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/CrossTU/TemplateCache.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Error.h"
#include <memory>
#include <optional>

namespace clang {
class ASTContext;
class CompilerInstance;
class Decl;
class FunctionDecl;
class Sema;
class TemplateArgumentList;
class TemplateDecl;

namespace cross_tu {
class CrossTranslationUnitContext;
class TemplateInstantiationCache;
}

namespace sema {

/// Configuration options for template caching in Sema
struct TemplateCacheConfig {
  /// Enable/disable template caching
  bool EnableCaching = false;

  /// Cross-TU directory for cache storage
  std::string CrossTUDir;

  /// Index file name for template cache
  std::string IndexName = "template-cache-index.txt";

  /// Maximum cache size in MB (0 = unlimited)
  size_t MaxCacheSizeMB = 0;

  /// Enable cache validation based on dependency changes
  bool ValidateDependencies = true;

  /// Enable caching of constraint satisfaction results
  bool CacheConstraints = true;

  /// Enable verbose cache logging
  bool VerboseLogging = false;
};

/// Statistics for template caching operations
struct TemplateCacheStats {
  size_t CacheHits = 0;
  size_t CacheMisses = 0;
  size_t CacheStores = 0;
  size_t CacheErrors = 0;
  size_t ConstraintCacheHits = 0;
  size_t ConstraintCacheMisses = 0;

  double getHitRate() const {
    size_t total = CacheHits + CacheMisses;
    return total > 0 ? static_cast<double>(CacheHits) / total : 0.0;
  }

  void reset() {
    *this = TemplateCacheStats{};
  }
};

/// RAII guard for template instantiation caching
class TemplateInstantiationCacheGuard {
public:
  TemplateInstantiationCacheGuard(Sema &S, const TemplateDecl *Template,
                                 const TemplateArgumentList &Args,
                                 SourceLocation POI);
  ~TemplateInstantiationCacheGuard();

  /// Check if a cached instantiation is available
  bool hasCachedInstantiation() const { return CachedDecl != nullptr; }

  /// Get the cached instantiation (if available)
  const Decl *getCachedInstantiation() const { return CachedDecl; }

  /// Mark the instantiation as completed for caching
  void markInstantiationCompleted(const Decl *InstantiatedDecl);

  /// Check if caching should be bypassed for this instantiation
  bool shouldBypassCache() const { return BypassCache; }

private:
  Sema &SemaRef;
  const TemplateDecl *Template;
  const TemplateArgumentList &Args;
  SourceLocation PointOfInstantiation;
  const Decl *CachedDecl = nullptr;
  bool BypassCache = false;
  bool InstantiationMarked = false;
};

/// Main interface for Sema template caching integration
class SemaTemplateCache {
public:
  SemaTemplateCache(Sema &S, const TemplateCacheConfig &Config);
  ~SemaTemplateCache();

  /// Initialize the cache with CTU context and CompilerInstance
  llvm::Error initialize(cross_tu::CrossTranslationUnitContext *CTUContext,
                        CompilerInstance &CI);

  /// Check if template caching is enabled
  bool isEnabled() const { return Config.EnableCaching && Cache != nullptr; }

  /// Try to get a cached template instantiation
  llvm::Expected<const Decl *> getCachedInstantiation(
      const ClassTemplateSpecializationDecl *Spec);

  llvm::Expected<const Decl *> getCachedInstantiation(
      const FunctionDecl *FD,
      const FunctionTemplateSpecializationInfo *Spec);

  llvm::Expected<const Decl *> getCachedInstantiation(
      const VarTemplateSpecializationDecl *Spec);

  /// Cache a completed template instantiation
  llvm::Error cacheInstantiation(const Decl *InstantiatedDecl,
                                const TemplateArgumentList &Args);

  /// Get cached constraint satisfaction result
  llvm::Expected<bool> getCachedConstraintSatisfaction(
      const ConceptDecl *Concept,
      const TemplateArgumentList &Args);

  /// Cache constraint satisfaction result
  llvm::Error cacheConstraintSatisfaction(const ConceptDecl *Concept,
                                         const TemplateArgumentList &Args,
                                         bool IsSatisfied);

  /// Check if a template should be cached
  bool shouldCacheTemplate(const TemplateDecl *Template) const;

  /// Check if template instantiation should use cache
  bool shouldUseCacheForInstantiation(const TemplateDecl *Template,
                                     const TemplateArgumentList &Args) const;

  /// Get cache statistics
  const TemplateCacheStats &getStats() const { return Stats; }

  /// Reset cache statistics
  void resetStats() { Stats.reset(); }

  /// Get cache configuration
  const TemplateCacheConfig &getConfig() const { return Config; }

  /// Update cache configuration
  void updateConfig(const TemplateCacheConfig &NewConfig);

  /// Handle cache errors
  void handleCacheError(const llvm::Error &Err, StringRef Context) const;

private:
  Sema &SemaRef;
  TemplateCacheConfig Config;
  TemplateCacheStats Stats;
  std::unique_ptr<cross_tu::TemplateInstantiationCache> Cache;

  /// Check if template is eligible for caching
  bool isTemplateEligibleForCaching(const TemplateDecl *Template) const;

  /// Log cache operation if verbose logging is enabled
  void logCacheOperation(StringRef Operation, StringRef Details) const;
};

/// Template instantiation interceptor that hooks into Sema's instantiation pipeline
class TemplateInstantiationInterceptor {
public:
  TemplateInstantiationInterceptor(Sema &S);

  /// Hook called before class template instantiation
  std::optional<const Decl *> beforeClassTemplateInstantiation(
      ClassTemplateSpecializationDecl *Spec,
      TemplateSpecializationKind TSK,
      SourceLocation POI);

  /// Hook called after class template instantiation
  void afterClassTemplateInstantiation(
      ClassTemplateSpecializationDecl *Spec,
      const Decl *InstantiatedDecl,
      TemplateSpecializationKind TSK);

  /// Hook called before function template instantiation
  std::optional<const Decl *> beforeFunctionTemplateInstantiation(
      FunctionDecl *FD,
      const FunctionTemplateSpecializationInfo *Spec,
      SourceLocation POI);

  /// Hook called after function template instantiation
  void afterFunctionTemplateInstantiation(
      FunctionDecl *FD,
      const FunctionTemplateSpecializationInfo *Spec,
      const Decl *InstantiatedDecl);

  /// Hook called before variable template instantiation
  std::optional<const Decl *> beforeVariableTemplateInstantiation(
      VarTemplateSpecializationDecl *Spec,
      TemplateSpecializationKind TSK,
      SourceLocation POI);

  /// Hook called after variable template instantiation
  void afterVariableTemplateInstantiation(
      VarTemplateSpecializationDecl *Spec,
      const Decl *InstantiatedDecl,
      TemplateSpecializationKind TSK);

private:
  Sema &SemaRef;
  SemaTemplateCache *getTemplateCache() const;
};

/// Utility functions for template cache integration

/// Create default template cache configuration from compiler options
TemplateCacheConfig createDefaultCacheConfig(const CompilerInstance &CI);

/// Check if template caching should be enabled based on compilation flags
bool shouldEnableTemplateCacheForSema(const CompilerInstance &CI);

/// Get template cache directory from environment or compiler options
std::string getTemplateCacheDirectoryForSema(const CompilerInstance &CI);

} // namespace sema
} // namespace clang

#endif // LLVM_CLANG_SEMA_TEMPLATEINSTANTIATIONCACHE_H