//===--- TemplateCaching.h - Template Instantiation Caching -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the template instantiation caching system.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_CROSSTU_TEMPLATECACHING_H
#define LLVM_CLANG_CROSSTU_TEMPLATECACHING_H

#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/Basic/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include <memory>
#include <string>
#include <vector>

namespace clang {

class ASTContext;
class ClassTemplateDecl;
class ClassTemplateSpecializationDecl;
class FunctionTemplateDecl;
class FunctionDecl;
class VarTemplateDecl;
class VarTemplateSpecializationDecl;

namespace cross_tu {

enum class template_cache_error_code {
  success = 0,
  cache_miss,
  serialization_error,
  deserialization_error,
  invalid_template_key,
  invalid_cache_format,
  cache_file_not_found,
  cache_directory_not_found,
  write_error,
  read_error
};

/// Error class for template caching operations
class TemplateCacheError : public llvm::ErrorInfo<TemplateCacheError> {
public:
  static char ID;
  TemplateCacheError(template_cache_error_code C) : Code(C) {}
  TemplateCacheError(template_cache_error_code C, std::string Message)
      : Code(C), ErrorMessage(std::move(Message)) {}

  void log(raw_ostream &OS) const override;
  std::error_code convertToErrorCode() const override;
  template_cache_error_code getCode() const { return Code; }
  StringRef getMessage() const { return ErrorMessage; }

private:
  template_cache_error_code Code;
  std::string ErrorMessage;
};

/// Represents a key for template instantiation cache lookup
class TemplateInstantiationKey {
public:
  /// Create a key from a template declaration and its arguments
  static llvm::Expected<TemplateInstantiationKey>
  create(const ClassTemplateSpecializationDecl *CTSD);

  static llvm::Expected<TemplateInstantiationKey>
  create(const FunctionDecl *FD);

  static llvm::Expected<TemplateInstantiationKey>
  create(const VarTemplateSpecializationDecl *VTSD);

  /// Get the string representation of the key
  StringRef getKeyString() const { return KeyString; }

  /// Get the template name
  StringRef getTemplateName() const { return TemplateName; }

  /// Get the template kind
  unsigned getTemplateKind() const { return TemplateKind; }

private:
  TemplateInstantiationKey(std::string Key, std::string Name, unsigned Kind)
      : KeyString(std::move(Key)), TemplateName(std::move(Name)),
        TemplateKind(Kind) {}

  std::string KeyString;
  std::string TemplateName;
  unsigned TemplateKind;
};

/// Represents a serialized template instantiation
class SerializedTemplateInstantiation {
public:
  SerializedTemplateInstantiation() = default;
  SerializedTemplateInstantiation(std::vector<char> Data)
      : Data(std::move(Data)) {}

  const std::vector<char> &getData() const { return Data; }
  std::vector<char> &getData() { return Data; }

private:
  std::vector<char> Data;
};

/// Manages the template instantiation cache
class TemplateInstantiationCache {
public:
  TemplateInstantiationCache(ASTContext &Context);
  ~TemplateInstantiationCache();

  /// Store a template instantiation in the cache
  llvm::Error cacheTemplateInstantiation(const ClassTemplateSpecializationDecl *CTSD);
  llvm::Error cacheTemplateInstantiation(const FunctionDecl *FD);
  llvm::Error cacheTemplateInstantiation(const VarTemplateSpecializationDecl *VTSD);

  /// Lookup a template instantiation in the cache
  llvm::Expected<const ClassTemplateSpecializationDecl *>
  lookupClassTemplateSpecialization(const ClassTemplateDecl *TD,
                                   ArrayRef<TemplateArgument> Args);

  llvm::Expected<const FunctionDecl *>
  lookupFunctionInstantiation(const FunctionTemplateDecl *TD,
                             ArrayRef<TemplateArgument> Args);

  llvm::Expected<const VarTemplateSpecializationDecl *>
  lookupVarTemplateSpecialization(const VarTemplateDecl *TD,
                                 ArrayRef<TemplateArgument> Args);

  /// Set the cache directory
  void setCacheDirectory(StringRef Dir) { CacheDirectory = Dir.str(); }

  /// Get the cache directory
  StringRef getCacheDirectory() const { return CacheDirectory; }

  /// Set the cache file prefix
  void setCacheFilePrefix(StringRef Prefix) { CacheFilePrefix = Prefix.str(); }

  /// Get the cache file prefix
  StringRef getCacheFilePrefix() const { return CacheFilePrefix; }

  /// Enable or disable the cache
  void setEnabled(bool Enable) { Enabled = Enable; }

  /// Check if the cache is enabled
  bool isEnabled() const { return Enabled; }

  /// Clear the cache
  void clear();

  /// Invalidate entries in the cache based on a predicate
  template <typename Pred>
  void invalidateIf(Pred P);

private:
  /// Serialize a template instantiation
  llvm::Expected<SerializedTemplateInstantiation>
  serializeTemplateInstantiation(const Decl *D);

  /// Deserialize a template instantiation
  llvm::Expected<Decl *>
  deserializeTemplateInstantiation(const SerializedTemplateInstantiation &STI);

  /// Get the cache file path for a template key
  std::string getCacheFilePath(const TemplateInstantiationKey &Key);

  /// Load a template instantiation from the cache file
  llvm::Expected<SerializedTemplateInstantiation>
  loadFromCacheFile(const TemplateInstantiationKey &Key);

  /// Save a template instantiation to the cache file
  llvm::Error saveToCacheFile(const TemplateInstantiationKey &Key,
                             const SerializedTemplateInstantiation &STI);

  ASTContext &Context;
  std::string CacheDirectory;
  std::string CacheFilePrefix;
  bool Enabled;

  // In-memory cache for faster lookups
  llvm::StringMap<SerializedTemplateInstantiation> MemoryCache;
};

} // namespace cross_tu
} // namespace clang

#endif // LLVM_CLANG_CROSSTU_TEMPLATECACHING_H
