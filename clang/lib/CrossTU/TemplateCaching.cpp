//===--- TemplateCaching.cpp - Template Instantiation Caching -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements the template instantiation caching system.
//
//===----------------------------------------------------------------------===//

#include "clang/CrossTU/TemplateCaching.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/FileManager.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <sstream>
#include <system_error>

namespace clang {
namespace cross_tu {

namespace {
// Error category for template cache errors
class TemplateCacheErrorCategory : public std::error_category {
public:
  const char *name() const noexcept override { return "clang.template_cache"; }

  std::string message(int Condition) const override {
    switch (static_cast<template_cache_error_code>(Condition)) {
    case template_cache_error_code::success:
      return "Success";
    case template_cache_error_code::cache_miss:
      return "Cache miss";
    case template_cache_error_code::serialization_error:
      return "Failed to serialize template instantiation";
    case template_cache_error_code::deserialization_error:
      return "Failed to deserialize template instantiation";
    case template_cache_error_code::invalid_template_key:
      return "Invalid template key";
    case template_cache_error_code::invalid_cache_format:
      return "Invalid cache format";
    case template_cache_error_code::cache_file_not_found:
      return "Cache file not found";
    case template_cache_error_code::cache_directory_not_found:
      return "Cache directory not found";
    case template_cache_error_code::write_error:
      return "Failed to write to cache file";
    case template_cache_error_code::read_error:
      return "Failed to read from cache file";
    }
    llvm_unreachable("Unrecognized template_cache_error_code");
  }
};

static llvm::ManagedStatic<TemplateCacheErrorCategory> Category;
} // end anonymous namespace

char TemplateCacheError::ID;

void TemplateCacheError::log(raw_ostream &OS) const {
  OS << Category->message(static_cast<int>(Code));
  if (!ErrorMessage.empty())
    OS << ": " << ErrorMessage;
  OS << '\n';
}

std::error_code TemplateCacheError::convertToErrorCode() const {
  return std::error_code(static_cast<int>(Code), *Category);
}

// Helper function to generate a mangled name for a template declaration
static std::string getMangledName(const NamedDecl *ND, ASTContext &Context) {
  std::unique_ptr<MangleContext> MC(Context.createMangleContext());
  if (!MC->shouldMangleDeclName(ND))
    return ND->getNameAsString();

  std::string MangledName;
  llvm::raw_string_ostream OS(MangledName);
  MC->mangleName(ND, OS);
  OS.flush();
  return MangledName;
}

// Helper function to generate a string representation of template arguments
static std::string getTemplateArgsString(ArrayRef<TemplateArgument> Args,
                                        ASTContext &Context) {
  std::string Result;
  llvm::raw_string_ostream OS(Result);

  for (const auto &Arg : Args) {
    OS << "<";
    Arg.print(Context.getPrintingPolicy(), OS);
    OS << ">";
  }

  return OS.str();
}

// Implementation of TemplateInstantiationKey::create for ClassTemplateSpecializationDecl
llvm::Expected<TemplateInstantiationKey>
TemplateInstantiationKey::create(const ClassTemplateSpecializationDecl *CTSD) {
  if (!CTSD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "Null ClassTemplateSpecializationDecl");

  ASTContext &Context = CTSD->getASTContext();
  ClassTemplateDecl *TD = CTSD->getSpecializedTemplate();
  if (!TD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "ClassTemplateSpecializationDecl has no specialized template");

  std::string MangledName = getMangledName(TD, Context);
  std::string ArgsStr = getTemplateArgsString(CTSD->getTemplateArgs().asArray(),
                                             Context);
  std::string Key = MangledName + ArgsStr;

  return TemplateInstantiationKey(Key, TD->getNameAsString(), 0);
}

// Implementation of TemplateInstantiationKey::create for FunctionDecl
llvm::Expected<TemplateInstantiationKey>
TemplateInstantiationKey::create(const FunctionDecl *FD) {
  if (!FD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key, "Null FunctionDecl");

  if (!FD->isTemplateInstantiation())
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "FunctionDecl is not a template instantiation");

  ASTContext &Context = FD->getASTContext();
  FunctionTemplateDecl *TD = FD->getPrimaryTemplate();
  if (!TD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "FunctionDecl has no primary template");

  std::string MangledName = getMangledName(TD, Context);
  std::string ArgsStr = getTemplateArgsString(
      FD->getTemplateSpecializationArgs()->asArray(), Context);
  std::string Key = MangledName + ArgsStr;

  return TemplateInstantiationKey(Key, TD->getNameAsString(), 1);
}

// Implementation of TemplateInstantiationKey::create for VarTemplateSpecializationDecl
llvm::Expected<TemplateInstantiationKey>
TemplateInstantiationKey::create(const VarTemplateSpecializationDecl *VTSD) {
  if (!VTSD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "Null VarTemplateSpecializationDecl");

  ASTContext &Context = VTSD->getASTContext();
  VarTemplateDecl *TD = VTSD->getSpecializedTemplate();
  if (!TD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "VarTemplateSpecializationDecl has no specialized template");

  std::string MangledName = getMangledName(TD, Context);
  std::string ArgsStr = getTemplateArgsString(VTSD->getTemplateArgs().asArray(),
                                             Context);
  std::string Key = MangledName + ArgsStr;

  return TemplateInstantiationKey(Key, TD->getNameAsString(), 2);
}

// Implementation of TemplateInstantiationCache constructor
TemplateInstantiationCache::TemplateInstantiationCache(ASTContext &Context)
    : Context(Context), CacheDirectory(".template-cache"),
      CacheFilePrefix("template-"), Enabled(false) {}

// Implementation of TemplateInstantiationCache destructor
TemplateInstantiationCache::~TemplateInstantiationCache() = default;

// Implementation of TemplateInstantiationCache::clear
void TemplateInstantiationCache::clear() {
  MemoryCache.clear();
}

// Implementation of TemplateInstantiationCache::getCacheFilePath
std::string
TemplateInstantiationCache::getCacheFilePath(const TemplateInstantiationKey &Key) {
  std::string FileName = CacheFilePrefix +
                        llvm::sys::path::filename(Key.getKeyString()).str() +
                        ".cache";
  SmallString<128> FilePath(CacheDirectory);
  llvm::sys::path::append(FilePath, FileName);
  return FilePath.str().str();
}

// Implementation of TemplateInstantiationCache::serializeTemplateInstantiation
llvm::Expected<SerializedTemplateInstantiation>
TemplateInstantiationCache::serializeTemplateInstantiation(const Decl *D) {
  if (!D)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::serialization_error, "Null declaration");

  // Use ASTWriter to serialize the declaration
  SmallVector<char, 1024> Buffer;
  llvm::BitstreamWriter Stream(Buffer);
  ASTWriter Writer(Stream);

  // Initialize the writer with the context
  Writer.WriteAST(Context, nullptr, nullptr, "", false);

  // Serialize the declaration
  Writer.AddDecl(const_cast<Decl *>(D));

  return SerializedTemplateInstantiation(std::vector<char>(Buffer.begin(), Buffer.end()));
}

// Implementation of TemplateInstantiationCache::deserializeTemplateInstantiation
llvm::Expected<Decl *>
TemplateInstantiationCache::deserializeTemplateInstantiation(
    const SerializedTemplateInstantiation &STI) {
  // Use ASTReader to deserialize the declaration
  const std::vector<char> &Buffer = STI.getData();
  if (Buffer.empty())
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::deserialization_error, "Empty buffer");

  // Create a memory buffer from the serialized data
  StringRef BufferRef(Buffer.data(), Buffer.size());
  std::unique_ptr<llvm::MemoryBuffer> MemBuffer =
      llvm::MemoryBuffer::getMemBuffer(BufferRef, "", false);

  // Create an ASTReader to deserialize the declaration
  ASTReader Reader(Context.getSourceManager(), Context.getFileManager(),
                  Context.getPCHContainerReader());

  // Read the serialized declaration
  // Note: This is a simplified version. In a real implementation, we would need
  // to handle more complex deserialization logic.

  // For now, return an error as this is a placeholder
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::deserialization_error,
      "Deserialization not fully implemented yet");
}

// Implementation of TemplateInstantiationCache::loadFromCacheFile
llvm::Expected<SerializedTemplateInstantiation>
TemplateInstantiationCache::loadFromCacheFile(const TemplateInstantiationKey &Key) {
  std::string FilePath = getCacheFilePath(Key);

  // Check if the file exists
  if (!llvm::sys::fs::exists(FilePath))
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::cache_file_not_found,
        "Cache file not found: " + FilePath);

  // Read the file
  std::ifstream File(FilePath, std::ios::binary);
  if (!File)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::read_error,
        "Failed to open cache file: " + FilePath);

  // Read the file content
  std::vector<char> Buffer((std::istreambuf_iterator<char>(File)),
                          std::istreambuf_iterator<char>());

  return SerializedTemplateInstantiation(std::move(Buffer));
}

// Implementation of TemplateInstantiationCache::saveToCacheFile
llvm::Error
TemplateInstantiationCache::saveToCacheFile(
    const TemplateInstantiationKey &Key,
    const SerializedTemplateInstantiation &STI) {
  // Create the cache directory if it doesn't exist
  if (!llvm::sys::fs::exists(CacheDirectory)) {
    std::error_code EC = llvm::sys::fs::create_directories(CacheDirectory);
    if (EC)
      return llvm::make_error<TemplateCacheError>(
          template_cache_error_code::cache_directory_not_found,
          "Failed to create cache directory: " + EC.message());
  }

  std::string FilePath = getCacheFilePath(Key);

  // Write the file
  std::ofstream File(FilePath, std::ios::binary);
  if (!File)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::write_error,
        "Failed to open cache file for writing: " + FilePath);

  // Write the content
  const std::vector<char> &Buffer = STI.getData();
  File.write(Buffer.data(), Buffer.size());

  if (!File)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::write_error,
        "Failed to write to cache file: " + FilePath);

  return llvm::Error::success();
}

// Implementation of TemplateInstantiationCache::cacheTemplateInstantiation for ClassTemplateSpecializationDecl
llvm::Error
TemplateInstantiationCache::cacheTemplateInstantiation(
    const ClassTemplateSpecializationDecl *CTSD) {
  if (!Enabled)
    return llvm::Error::success();

  // Create a key for the template instantiation
  auto KeyOrErr = TemplateInstantiationKey::create(CTSD);
  if (!KeyOrErr)
    return KeyOrErr.takeError();

  // Serialize the template instantiation
  auto STIOrErr = serializeTemplateInstantiation(CTSD);
  if (!STIOrErr)
    return STIOrErr.takeError();

  // Add to the memory cache
  MemoryCache[KeyOrErr->getKeyString().str()] = std::move(*STIOrErr);

  // Save to the cache file
  return saveToCacheFile(*KeyOrErr, MemoryCache[KeyOrErr->getKeyString().str()]);
}

// Implementation of TemplateInstantiationCache::cacheTemplateInstantiation for FunctionDecl
llvm::Error
TemplateInstantiationCache::cacheTemplateInstantiation(const FunctionDecl *FD) {
  if (!Enabled)
    return llvm::Error::success();

  // Create a key for the template instantiation
  auto KeyOrErr = TemplateInstantiationKey::create(FD);
  if (!KeyOrErr)
    return KeyOrErr.takeError();

  // Serialize the template instantiation
  auto STIOrErr = serializeTemplateInstantiation(FD);
  if (!STIOrErr)
    return STIOrErr.takeError();

  // Add to the memory cache
  MemoryCache[KeyOrErr->getKeyString().str()] = std::move(*STIOrErr);

  // Save to the cache file
  return saveToCacheFile(*KeyOrErr, MemoryCache[KeyOrErr->getKeyString().str()]);
}

// Implementation of TemplateInstantiationCache::cacheTemplateInstantiation for VarTemplateSpecializationDecl
llvm::Error
TemplateInstantiationCache::cacheTemplateInstantiation(
    const VarTemplateSpecializationDecl *VTSD) {
  if (!Enabled)
    return llvm::Error::success();

  // Create a key for the template instantiation
  auto KeyOrErr = TemplateInstantiationKey::create(VTSD);
  if (!KeyOrErr)
    return KeyOrErr.takeError();

  // Serialize the template instantiation
  auto STIOrErr = serializeTemplateInstantiation(VTSD);
  if (!STIOrErr)
    return STIOrErr.takeError();

  // Add to the memory cache
  MemoryCache[KeyOrErr->getKeyString().str()] = std::move(*STIOrErr);

  // Save to the cache file
  return saveToCacheFile(*KeyOrErr, MemoryCache[KeyOrErr->getKeyString().str()]);
}

// Implementation of TemplateInstantiationCache::lookupClassTemplateSpecialization
llvm::Expected<const ClassTemplateSpecializationDecl *>
TemplateInstantiationCache::lookupClassTemplateSpecialization(
    const ClassTemplateDecl *TD, ArrayRef<TemplateArgument> Args) {
  if (!Enabled)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::cache_miss, "Cache is disabled");

  if (!TD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "Null ClassTemplateDecl");

  // Create a temporary specialization to generate the key
  ClassTemplateSpecializationDecl *CTSD =
      ClassTemplateSpecializationDecl::Create(
          Context, TTK_Class, TD->getDeclContext(), TD->getLocation(),
          TD->getLocation(), TD, nullptr, nullptr);

  // Set the template arguments
  CTSD->setTemplateArgs(TemplateArgumentList::CreateCopy(Context, Args));

  // Create a key for the template instantiation
  auto KeyOrErr = TemplateInstantiationKey::create(CTSD);
  if (!KeyOrErr) {
    // Clean up the temporary declaration
    CTSD->setInvalidDecl();
    return KeyOrErr.takeError();
  }

  // Clean up the temporary declaration
  CTSD->setInvalidDecl();

  // Check the memory cache first
  auto It = MemoryCache.find(KeyOrErr->getKeyString().str());
  if (It != MemoryCache.end()) {
    // Deserialize the template instantiation
    auto DeclOrErr = deserializeTemplateInstantiation(It->second);
    if (!DeclOrErr)
      return DeclOrErr.takeError();

    // Cast to the expected type
    if (auto *Result = dyn_cast<ClassTemplateSpecializationDecl>(*DeclOrErr))
      return Result;

    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::deserialization_error,
        "Deserialized declaration is not a ClassTemplateSpecializationDecl");
  }

  // If not in memory cache, try to load from the cache file
  auto STIOrErr = loadFromCacheFile(*KeyOrErr);
  if (!STIOrErr)
    return STIOrErr.takeError();

  // Add to the memory cache
  MemoryCache[KeyOrErr->getKeyString().str()] = std::move(*STIOrErr);

  // Deserialize the template instantiation
  auto DeclOrErr = deserializeTemplateInstantiation(
      MemoryCache[KeyOrErr->getKeyString().str()]);
  if (!DeclOrErr)
    return DeclOrErr.takeError();

  // Cast to the expected type
  if (auto *Result = dyn_cast<ClassTemplateSpecializationDecl>(*DeclOrErr))
    return Result;

  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::deserialization_error,
      "Deserialized declaration is not a ClassTemplateSpecializationDecl");
}

// Implementation of TemplateInstantiationCache::lookupFunctionInstantiation
llvm::Expected<const FunctionDecl *>
TemplateInstantiationCache::lookupFunctionInstantiation(
    const FunctionTemplateDecl *TD, ArrayRef<TemplateArgument> Args) {
  if (!Enabled)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::cache_miss, "Cache is disabled");

  if (!TD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "Null FunctionTemplateDecl");

  // Create a key for the template instantiation
  // Note: This is a simplified version. In a real implementation, we would need
  // to create a temporary FunctionDecl with the given template arguments.

  // For now, return an error as this is a placeholder
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::cache_miss,
      "Function template lookup not fully implemented yet");
}

// Implementation of TemplateInstantiationCache::lookupVarTemplateSpecialization
llvm::Expected<const VarTemplateSpecializationDecl *>
TemplateInstantiationCache::lookupVarTemplateSpecialization(
    const VarTemplateDecl *TD, ArrayRef<TemplateArgument> Args) {
  if (!Enabled)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::cache_miss, "Cache is disabled");

  if (!TD)
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_key,
        "Null VarTemplateDecl");

  // Create a key for the template instantiation
  // Note: This is a simplified version. In a real implementation, we would need
  // to create a temporary VarTemplateSpecializationDecl with the given template arguments.

  // For now, return an error as this is a placeholder
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::cache_miss,
      "Variable template lookup not fully implemented yet");
}

} // namespace cross_tu
} // namespace clang
