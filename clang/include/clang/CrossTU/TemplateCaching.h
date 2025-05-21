//===--- TemplateCaching.h - Template Instantiation Caching -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines the template instantiation caching system, which reduces
//  compilation time by caching and reusing template instantiations.
//
//  The template caching system provides the following functionality:
//  - Serialization and deserialization of template instantiations
//  - In-memory and disk-based caching of template instantiations
//  - Lookup of cached template instantiations based on template name and arguments
//
//  The system supports caching of:
//  - Class template specializations
//  - Function template instantiations
//  - Variable template specializations
//
//  To enable template caching, use the -ftemplate-caching command-line option.
//  Additional options:
//  - -ftemplate-cache-dir=<dir>: Set the directory for cache files
//  - -ftemplate-cache-prefix=<prefix>: Set the prefix for cache files
//
//  See the TemplateCaching.rst documentation for more details.
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

/// Error class for template caching operations.
/// Represents errors that can occur during template caching operations,
/// such as serialization/deserialization errors, cache misses, or I/O errors.
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

/// Represents a key for template instantiation cache lookup.
/// A template instantiation key uniquely identifies a template instantiation
/// based on the template declaration and its template arguments.
/// This key is used for both storing and retrieving template instantiations
/// from the cache.
class TemplateInstantiationKey {
public:
  /// Create a key from a class template specialization declaration.
  /// This method generates a unique key based on the template name and arguments.
  /// \param CTSD The class template specialization declaration
  /// \return A key that can be used for cache lookup, or an error
  static llvm::Expected<TemplateInstantiationKey>
  create(const ClassTemplateSpecializationDecl *CTSD);

  /// Create a key from a function declaration that is a template instantiation.
  /// \param FD The function declaration (must be a template instantiation)
  /// \return A key that can be used for cache lookup, or an error
  static llvm::Expected<TemplateInstantiationKey>
  create(const FunctionDecl *FD);

  /// Create a key from a variable template specialization declaration.
  /// \param VTSD The variable template specialization declaration
  /// \return A key that can be used for cache lookup, or an error
  static llvm::Expected<TemplateInstantiationKey>
  create(const VarTemplateSpecializationDecl *VTSD);

  /// Get the string representation of the key.
  /// This string uniquely identifies the template instantiation.
  /// \return The string representation of the key
  StringRef getKeyString() const { return KeyString; }

  /// Get the template name.
  /// \return The name of the template
  StringRef getTemplateName() const { return TemplateName; }

  /// Get the template kind.
  /// \return The kind of template (0 for class, 1 for function, 2 for variable)
  unsigned getTemplateKind() const { return TemplateKind; }

private:
  TemplateInstantiationKey(std::string Key, std::string Name, unsigned Kind)
      : KeyString(std::move(Key)), TemplateName(std::move(Name)),
        TemplateKind(Kind) {}

  std::string KeyString;
  std::string TemplateName;
  unsigned TemplateKind;
};

/// Represents a serialized template instantiation.
/// This class holds the binary data of a serialized template instantiation
/// that can be stored in the cache and later deserialized.
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

/// Manages the template instantiation cache.
/// This class provides the main interface for the template caching system.
/// It handles storing and retrieving template instantiations from both
/// an in-memory cache and a disk-based cache.
class TemplateInstantiationCache {
public:
  TemplateInstantiationCache(ASTContext &Context);
  ~TemplateInstantiationCache();

  /// Store a class template specialization in the cache.
  /// This method serializes the template instantiation and stores it in both
  /// the in-memory cache and the disk-based cache.
  /// \param CTSD The class template specialization declaration to cache
  /// \return Success or an error describing what went wrong
  llvm::Error cacheTemplateInstantiation(const ClassTemplateSpecializationDecl *CTSD);

  /// Store a function template instantiation in the cache.
  /// \param FD The function declaration (must be a template instantiation)
  /// \return Success or an error describing what went wrong
  llvm::Error cacheTemplateInstantiation(const FunctionDecl *FD);

  /// Store a variable template specialization in the cache.
  /// \param VTSD The variable template specialization declaration to cache
  /// \return Success or an error describing what went wrong
  llvm::Error cacheTemplateInstantiation(const VarTemplateSpecializationDecl *VTSD);

  /// Lookup a class template specialization in the cache.
  /// This method checks both the in-memory cache and the disk-based cache
  /// for a matching template instantiation.
  /// \param TD The class template declaration
  /// \param Args The template arguments
  /// \return The cached template instantiation or an error
  llvm::Expected<const ClassTemplateSpecializationDecl *>
  lookupClassTemplateSpecialization(const ClassTemplateDecl *TD,
                                   ArrayRef<TemplateArgument> Args);

  /// Lookup a function template instantiation in the cache.
  /// \param TD The function template declaration
  /// \param Args The template arguments
  /// \return The cached template instantiation or an error
  llvm::Expected<const FunctionDecl *>
  lookupFunctionInstantiation(const FunctionTemplateDecl *TD,
                             ArrayRef<TemplateArgument> Args);

  /// Lookup a variable template specialization in the cache.
  /// \param TD The variable template declaration
  /// \param Args The template arguments
  /// \return The cached template instantiation or an error
  llvm::Expected<const VarTemplateSpecializationDecl *>
  lookupVarTemplateSpecialization(const VarTemplateDecl *TD,
                                 ArrayRef<TemplateArgument> Args);

  /// Set the directory where cache files are stored.
  /// \param Dir The path to the cache directory
  void setCacheDirectory(StringRef Dir) { CacheDirectory = Dir.str(); }

  /// Get the directory where cache files are stored.
  /// \return The path to the cache directory
  StringRef getCacheDirectory() const { return CacheDirectory; }

  /// Set the prefix for cache files.
  /// This prefix is prepended to cache file names to avoid conflicts.
  /// \param Prefix The prefix for cache files
  void setCacheFilePrefix(StringRef Prefix) { CacheFilePrefix = Prefix.str(); }

  /// Get the prefix for cache files.
  /// \return The prefix for cache files
  StringRef getCacheFilePrefix() const { return CacheFilePrefix; }

  /// Enable or disable the cache.
  /// When disabled, cache lookups will always return cache misses and
  /// caching operations will be no-ops.
  /// \param Enable True to enable the cache, false to disable
  void setEnabled(bool Enable) { Enabled = Enable; }

  /// Check if the cache is enabled.
  /// \return True if the cache is enabled, false otherwise
  bool isEnabled() const { return Enabled; }

  /// Clear the in-memory cache.
  /// This does not affect the disk-based cache.
  void clear();

  /// Invalidate entries in the in-memory cache based on a predicate.
  /// This method removes entries from the in-memory cache that match the
  /// given predicate. It does not affect the disk-based cache.
  /// \param P A predicate function that takes a key and returns true if the
  ///          entry should be invalidated
  template <typename Pred>
  void invalidateIf(Pred P);

private:
  /// Serialize a template instantiation to binary format.
  /// This method uses the ASTWriter to serialize the declaration into a binary format
  /// that can be stored in the cache.
  /// \param D The declaration to serialize (must be a template instantiation)
  /// \return The serialized template instantiation or an error
  llvm::Expected<SerializedTemplateInstantiation>
  serializeTemplateInstantiation(const Decl *D);

  /// Deserialize a template instantiation from binary format.
  /// This method uses the ASTReader to deserialize the declaration from the binary format
  /// stored in the cache.
  /// \param STI The serialized template instantiation
  /// \return The deserialized declaration or an error
  llvm::Expected<Decl *>
  deserializeTemplateInstantiation(const SerializedTemplateInstantiation &STI);

  /// Get the file path for a cache file based on the template key.
  /// This method constructs the path to the cache file using the cache directory,
  /// the cache file prefix, and a hash of the template key.
  /// \param Key The template instantiation key
  /// \return The path to the cache file
  std::string getCacheFilePath(const TemplateInstantiationKey &Key);

  /// Load a template instantiation from the disk-based cache.
  /// This method reads the serialized template instantiation from the cache file.
  /// \param Key The template instantiation key
  /// \return The serialized template instantiation or an error
  llvm::Expected<SerializedTemplateInstantiation>
  loadFromCacheFile(const TemplateInstantiationKey &Key);

  /// Save a template instantiation to the disk-based cache.
  /// This method writes the serialized template instantiation to the cache file.
  /// \param Key The template instantiation key
  /// \param STI The serialized template instantiation
  /// \return Success or an error describing what went wrong
  llvm::Error saveToCacheFile(const TemplateInstantiationKey &Key,
                             const SerializedTemplateInstantiation &STI);

  /// The AST context used for serialization and deserialization
  ASTContext &Context;

  /// The directory where cache files are stored
  std::string CacheDirectory;

  /// The prefix for cache files
  std::string CacheFilePrefix;

  /// Whether the cache is enabled
  bool Enabled;

  /// In-memory cache for faster lookups during a single compilation session
  llvm::StringMap<SerializedTemplateInstantiation> MemoryCache;
};

} // namespace cross_tu
} // namespace clang

#endif // LLVM_CLANG_CROSSTU_TEMPLATECACHING_H
