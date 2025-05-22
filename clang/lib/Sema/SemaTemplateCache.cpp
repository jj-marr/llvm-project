//===--- SemaTemplateCache.cpp - Sema Template Caching Integration ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements the integration between the template caching system
//  and Clang's Sema (semantic analysis) phase to automatically intercept
//  and cache template instantiations during compilation.
//
//===----------------------------------------------------------------------===//

#include "clang/Sema/TemplateInstantiationCache.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Basic/Linkage.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/FileManager.h"
#include "clang/CrossTU/CrossTranslationUnit.h"
#include "clang/CrossTU/TemplateCache.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::sema;

//===----------------------------------------------------------------------===//
// TemplateInstantiationCacheGuard Implementation
//===----------------------------------------------------------------------===//

TemplateInstantiationCacheGuard::TemplateInstantiationCacheGuard(
    Sema &S, const TemplateDecl *Template, const TemplateArgumentList &Args,
    SourceLocation POI)
    : SemaRef(S), Template(Template), Args(Args), PointOfInstantiation(POI) {

  // Check if template caching is available
  if (!SemaRef.getTemplateCache() || !SemaRef.getTemplateCache()->isEnabled()) {
    BypassCache = true;
    return;
  }

  // Check if this template should be cached
  if (!SemaRef.getTemplateCache()->shouldUseCacheForInstantiation(Template, Args)) {
    BypassCache = true;
    return;
  }

  // Try to get cached instantiation
  if (auto *ClassSpec = dyn_cast<ClassTemplateSpecializationDecl>(Template)) {
    if (auto CachedResult = SemaRef.getTemplateCache()->getCachedInstantiation(ClassSpec)) {
      CachedDecl = *CachedResult;
    } else {
      SemaRef.getTemplateCache()->handleCacheError(CachedResult.takeError(),
                                                  "getCachedInstantiation");
    }
  }
  // Add similar handling for function and variable templates as needed
}

TemplateInstantiationCacheGuard::~TemplateInstantiationCacheGuard() {
  // Destructor - cleanup if needed
}

void TemplateInstantiationCacheGuard::markInstantiationCompleted(
    const Decl *InstantiatedDecl) {
  if (BypassCache || InstantiationMarked || !SemaRef.getTemplateCache())
    return;

  InstantiationMarked = true;

  // Cache the completed instantiation
  if (auto Err = SemaRef.getTemplateCache()->cacheInstantiation(InstantiatedDecl, Args)) {
    SemaRef.getTemplateCache()->handleCacheError(std::move(Err),
                                                "cacheInstantiation");
  }
}

//===----------------------------------------------------------------------===//
// SemaTemplateCache Implementation
//===----------------------------------------------------------------------===//

SemaTemplateCache::SemaTemplateCache(Sema &S, const TemplateCacheConfig &Config)
    : SemaRef(S), Config(Config) {}

SemaTemplateCache::~SemaTemplateCache() = default;

llvm::Error SemaTemplateCache::initialize(
    cross_tu::CrossTranslationUnitContext *CTUContext,
    CompilerInstance &CI) {
  if (!Config.EnableCaching)
    return llvm::Error::success();

  if (!CTUContext)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                  "CTU context required for template caching");

  // Create the template cache instance
  Cache = std::make_unique<cross_tu::TemplateInstantiationCache>(
      CI, *CTUContext);

  logCacheOperation("initialize", "Template cache initialized successfully");
  return llvm::Error::success();
}

llvm::Expected<const Decl *> SemaTemplateCache::getCachedInstantiation(
    const ClassTemplateSpecializationDecl *Spec) {
  if (!isEnabled())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                  "Template cache not enabled");

  Stats.CacheMisses++; // Will be corrected if we find a hit

  auto Result = Cache->getCachedTemplateInstantiation(
      Spec, Config.CrossTUDir, Config.IndexName);

  if (Result) {
    Stats.CacheMisses--; // Correct the miss count
    Stats.CacheHits++;
    logCacheOperation("cache_hit", "Class template specialization found in cache");
    return *Result;
  } else {
    logCacheOperation("cache_miss", "Class template specialization not in cache");
    return Result.takeError();
  }
}

llvm::Expected<const Decl *> SemaTemplateCache::getCachedInstantiation(
    const FunctionDecl *FD, const FunctionTemplateSpecializationInfo *Spec) {
  if (!isEnabled())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                  "Template cache not enabled");

  Stats.CacheMisses++; // Will be corrected if we find a hit

  auto Result = Cache->getCachedTemplateInstantiation(
      FD, Spec, Config.CrossTUDir, Config.IndexName);

  if (Result) {
    Stats.CacheMisses--; // Correct the miss count
    Stats.CacheHits++;
    logCacheOperation("cache_hit", "Function template specialization found in cache");
    return *Result;
  } else {
    logCacheOperation("cache_miss", "Function template specialization not in cache");
    return Result.takeError();
  }
}

llvm::Expected<const Decl *> SemaTemplateCache::getCachedInstantiation(
    const VarTemplateSpecializationDecl *Spec) {
  if (!isEnabled())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                  "Template cache not enabled");

  Stats.CacheMisses++; // Will be corrected if we find a hit

  auto Result = Cache->getCachedTemplateInstantiation(
      Spec, Config.CrossTUDir, Config.IndexName);

  if (Result) {
    Stats.CacheMisses--; // Correct the miss count
    Stats.CacheHits++;
    logCacheOperation("cache_hit", "Variable template specialization found in cache");
    return *Result;
  } else {
    logCacheOperation("cache_miss", "Variable template specialization not in cache");
    return Result.takeError();
  }
}

llvm::Error SemaTemplateCache::cacheInstantiation(
    const Decl *InstantiatedDecl, const TemplateArgumentList &Args) {
  if (!isEnabled())
    return llvm::Error::success(); // Silently succeed if caching disabled

  Stats.CacheStores++;

  auto Result = Cache->cacheTemplateInstantiation(
      InstantiatedDecl, Args, Config.CrossTUDir, Config.IndexName);

  if (Result) {
    logCacheOperation("cache_store", "Template instantiation cached successfully");
    return llvm::Error::success();
  } else {
    Stats.CacheErrors++;
    logCacheOperation("cache_error", "Failed to cache template instantiation");
    return Result;
  }
}

llvm::Expected<bool> SemaTemplateCache::getCachedConstraintSatisfaction(
    const ConceptDecl *Concept, const TemplateArgumentList &Args) {
  if (!isEnabled() || !Config.CacheConstraints)
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                  "Constraint caching not enabled");

  Stats.ConstraintCacheMisses++; // Will be corrected if we find a hit

  auto Result = Cache->getCachedConstraintSatisfaction(
      Concept, Args, Config.CrossTUDir, Config.IndexName);

  if (Result) {
    Stats.ConstraintCacheMisses--; // Correct the miss count
    Stats.ConstraintCacheHits++;
    logCacheOperation("constraint_cache_hit", "Constraint satisfaction found in cache");
    return *Result;
  } else {
    logCacheOperation("constraint_cache_miss", "Constraint satisfaction not in cache");
    return Result.takeError();
  }
}

llvm::Error SemaTemplateCache::cacheConstraintSatisfaction(
    const ConceptDecl *Concept, const TemplateArgumentList &Args,
    bool IsSatisfied) {
  if (!isEnabled() || !Config.CacheConstraints)
    return llvm::Error::success(); // Silently succeed if caching disabled

  auto Result = Cache->cacheConstraintSatisfaction(
      Concept, Args, IsSatisfied, Config.CrossTUDir, Config.IndexName);

  if (Result) {
    logCacheOperation("constraint_cache_store",
                     "Constraint satisfaction cached successfully");
    return llvm::Error::success();
  } else {
    Stats.CacheErrors++;
    logCacheOperation("constraint_cache_error",
                     "Failed to cache constraint satisfaction");
    return Result;
  }
}

bool SemaTemplateCache::shouldCacheTemplate(const TemplateDecl *Template) const {
  if (!isEnabled())
    return false;

  return isTemplateEligibleForCaching(Template);
}

bool SemaTemplateCache::shouldUseCacheForInstantiation(
    const TemplateDecl *Template, const TemplateArgumentList &Args) const {
  if (!isEnabled())
    return false;

  // Don't cache templates with dependent arguments during parsing
  if (SemaRef.ParsingInitForAutoVars.size() > 0)
    return false;

  return isTemplateEligibleForCaching(Template);
}

void SemaTemplateCache::updateConfig(const TemplateCacheConfig &NewConfig) {
  Config = NewConfig;

  // Reinitialize cache if needed
  if (Config.EnableCaching && !Cache) {
    // Note: This would require access to CTU context, which might not be available
    logCacheOperation("config_update", "Cache configuration updated");
  }
}

bool SemaTemplateCache::isTemplateEligibleForCaching(
    const TemplateDecl *Template) const {
  if (!Template)
    return false;

  // Don't cache templates that are currently being instantiated
  // (to avoid infinite recursion)
  if (SemaRef.CodeSynthesisContexts.size() > 0) {
    for (const auto &Context : SemaRef.CodeSynthesisContexts) {
      if (Context.Template == Template)
        return false;
    }
  }

  // Don't cache local templates
  if (Template->getDeclContext()->isFunctionOrMethod())
    return false;

  // Don't cache templates with local linkage
  if (Template->getLinkageInternal() == Linkage::Internal)
    return false;

  return true;
}

void SemaTemplateCache::logCacheOperation(StringRef Operation,
                                         StringRef Details) const {
  if (!Config.VerboseLogging)
    return;

  llvm::errs() << "TemplateCache[" << Operation << "]: " << Details << "\n";
}

void SemaTemplateCache::handleCacheError(const llvm::Error &Err,
                                        StringRef Context) const {
  const_cast<TemplateCacheStats&>(Stats).CacheErrors++;

  if (Config.VerboseLogging) {
    std::string ErrorMsg;
    llvm::raw_string_ostream OS(ErrorMsg);
    OS << Err;
    std::string FullMsg = Context.str() + ": " + ErrorMsg;
    logCacheOperation("error", FullMsg);
  }

  // For now, we consume the error and continue compilation
  // In the future, we might want to emit diagnostics
  llvm::consumeError(std::move(const_cast<llvm::Error&>(Err)));
}

//===----------------------------------------------------------------------===//
// TemplateInstantiationInterceptor Implementation
//===----------------------------------------------------------------------===//

TemplateInstantiationInterceptor::TemplateInstantiationInterceptor(Sema &S)
    : SemaRef(S) {}

std::optional<const Decl *>
TemplateInstantiationInterceptor::beforeClassTemplateInstantiation(
    ClassTemplateSpecializationDecl *Spec, TemplateSpecializationKind TSK,
    SourceLocation POI) {

  auto *Cache = getTemplateCache();
  if (!Cache || !Cache->isEnabled())
    return std::nullopt;

  if (auto CachedResult = Cache->getCachedInstantiation(Spec)) {
    return *CachedResult;
  } else {
    Cache->handleCacheError(CachedResult.takeError(),
                           "beforeClassTemplateInstantiation");
    return std::nullopt;
  }
}

void TemplateInstantiationInterceptor::afterClassTemplateInstantiation(
    ClassTemplateSpecializationDecl *Spec, const Decl *InstantiatedDecl,
    TemplateSpecializationKind TSK) {

  auto *Cache = getTemplateCache();
  if (!Cache || !Cache->isEnabled())
    return;

  // Cache the instantiated declaration
  const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
  if (auto Err = Cache->cacheInstantiation(InstantiatedDecl, TemplateArgs)) {
    Cache->handleCacheError(std::move(Err), "afterClassTemplateInstantiation");
  }
}

std::optional<const Decl *>
TemplateInstantiationInterceptor::beforeFunctionTemplateInstantiation(
    FunctionDecl *FD, const FunctionTemplateSpecializationInfo *Spec,
    SourceLocation POI) {

  auto *Cache = getTemplateCache();
  if (!Cache || !Cache->isEnabled())
    return std::nullopt;

  if (auto CachedResult = Cache->getCachedInstantiation(FD, Spec)) {
    return *CachedResult;
  } else {
    Cache->handleCacheError(CachedResult.takeError(),
                           "beforeFunctionTemplateInstantiation");
    return std::nullopt;
  }
}

void TemplateInstantiationInterceptor::afterFunctionTemplateInstantiation(
    FunctionDecl *FD, const FunctionTemplateSpecializationInfo *Spec,
    const Decl *InstantiatedDecl) {

  auto *Cache = getTemplateCache();
  if (!Cache || !Cache->isEnabled())
    return;

  // Cache the instantiated declaration
  if (Spec && Spec->TemplateArguments) {
    if (auto Err = Cache->cacheInstantiation(InstantiatedDecl,
                                           *Spec->TemplateArguments)) {
      Cache->handleCacheError(std::move(Err), "afterFunctionTemplateInstantiation");
    }
  }
}

std::optional<const Decl *>
TemplateInstantiationInterceptor::beforeVariableTemplateInstantiation(
    VarTemplateSpecializationDecl *Spec, TemplateSpecializationKind TSK,
    SourceLocation POI) {

  auto *Cache = getTemplateCache();
  if (!Cache || !Cache->isEnabled())
    return std::nullopt;

  if (auto CachedResult = Cache->getCachedInstantiation(Spec)) {
    return *CachedResult;
  } else {
    Cache->handleCacheError(CachedResult.takeError(),
                           "beforeVariableTemplateInstantiation");
    return std::nullopt;
  }
}

void TemplateInstantiationInterceptor::afterVariableTemplateInstantiation(
    VarTemplateSpecializationDecl *Spec, const Decl *InstantiatedDecl,
    TemplateSpecializationKind TSK) {

  auto *Cache = getTemplateCache();
  if (!Cache || !Cache->isEnabled())
    return;

  // Cache the instantiated declaration
  const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
  if (auto Err = Cache->cacheInstantiation(InstantiatedDecl, TemplateArgs)) {
    Cache->handleCacheError(std::move(Err), "afterVariableTemplateInstantiation");
  }
}

SemaTemplateCache *TemplateInstantiationInterceptor::getTemplateCache() const {
  return SemaRef.getTemplateCache();
}

//===----------------------------------------------------------------------===//
// Utility Functions Implementation
//===----------------------------------------------------------------------===//

TemplateCacheConfig createDefaultCacheConfig(const CompilerInstance &CI) {
  TemplateCacheConfig Config;

  // Check if template caching should be enabled
  Config.EnableCaching = shouldEnableTemplateCacheForSema(CI);

  // Set cache directory
  Config.CrossTUDir = getTemplateCacheDirectoryForSema(CI);

  // Configure based on compiler options
  const auto &LangOpts = CI.getLangOpts();

  // Enable constraint caching for C++20 and later
  Config.CacheConstraints = LangOpts.CPlusPlus20;

  // Enable dependency validation in debug builds
  Config.ValidateDependencies = !CI.getCodeGenOpts().OptimizationLevel;

  // Enable verbose logging if requested
  Config.VerboseLogging = CI.getDiagnosticOpts().ShowColors; // Placeholder

  return Config;
}

bool shouldEnableTemplateCacheForSema(const CompilerInstance &CI) {
  // Enable template caching if:
  // 1. We're not in syntax-only mode
  // 2. We have CTU analysis enabled
  // 3. Template caching is not explicitly disabled

  const auto &FrontendOpts = CI.getFrontendOpts();

  // Don't enable for syntax-only compilation
  if (FrontendOpts.ProgramAction == frontend::ParseSyntaxOnly)
    return false;

  // Check for explicit disable flag (would need to be added to driver)
  // For now, enable by default in appropriate contexts
  return true;
}

std::string getTemplateCacheDirectoryForSema(const CompilerInstance &CI) {
  // Try environment variable first
  if (const char *CacheDir = std::getenv("CLANG_TEMPLATE_CACHE_DIR"))
    return std::string(CacheDir);

  // Use temporary directory as fallback
  llvm::SmallString<256> TempDir;
  llvm::sys::path::system_temp_directory(true, TempDir);
  llvm::sys::path::append(TempDir, "clang-template-cache");

  return std::string(TempDir);
}

//===----------------------------------------------------------------------===//
// Sema Integration Methods Implementation
//===----------------------------------------------------------------------===//

namespace clang {

llvm::Error Sema::initializeTemplateCache(
    const sema::TemplateCacheConfig &Config,
    cross_tu::CrossTranslationUnitContext *CTUContext,
    CompilerInstance &CI) {

  if (!Config.EnableCaching)
    return llvm::Error::success();

  // Create template cache instance
  TemplateCache = std::make_unique<sema::SemaTemplateCache>(*this, Config);

  // Initialize the cache
  if (auto Err = TemplateCache->initialize(CTUContext, CI))
    return Err;

  // Create template interceptor
  TemplateInterceptor = std::make_unique<sema::TemplateInstantiationInterceptor>(*this);

  return llvm::Error::success();
}

const sema::TemplateCacheStats &Sema::getTemplateCacheStats() const {
  static sema::TemplateCacheStats EmptyStats;
  if (!TemplateCache)
    return EmptyStats;
  return TemplateCache->getStats();
}

void Sema::resetTemplateCacheStats() {
  if (TemplateCache)
    TemplateCache->resetStats();
}

} // namespace clang