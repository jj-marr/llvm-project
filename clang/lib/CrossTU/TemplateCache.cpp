//===--- TemplateCache.cpp - Template Caching for CTU ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements template caching functionality for Cross-Translation
//  Unit analysis.
//
//===----------------------------------------------------------------------===//

#include "clang/CrossTU/TemplateCache.h"
#include "clang/AST/ASTImporter.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Index/USRGeneration.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <sstream>

namespace clang {
namespace cross_tu {

char TemplateCacheError::ID = 0;

void TemplateCacheError::log(raw_ostream &OS) const {
  switch (Code) {
  case template_cache_error_code::success:
    OS << "Success";
    break;
  case template_cache_error_code::unspecified:
    OS << "An unknown template cache error has occurred";
    break;
  case template_cache_error_code::invalid_template_usr:
    OS << "Invalid template USR";
    break;
  case template_cache_error_code::template_instantiation_failed:
    OS << "Template instantiation failed";
    break;
  case template_cache_error_code::template_cache_corrupted:
    OS << "Template cache is corrupted";
    break;
  case template_cache_error_code::template_argument_mismatch:
    OS << "Template argument mismatch";
    break;
  case template_cache_error_code::constraint_evaluation_failed:
    OS << "Constraint evaluation failed";
    break;
  case template_cache_error_code::template_not_found_in_cache:
    OS << "Template not found in cache";
    break;
  case template_cache_error_code::template_cache_write_failed:
    OS << "Failed to write to template cache";
    break;
  case template_cache_error_code::template_dependency_changed:
    OS << "Template dependency has changed";
    break;
  }
  if (!Message.empty())
    OS << ": " << Message;
}

std::error_code TemplateCacheError::convertToErrorCode() const {
  return std::make_error_code(std::errc::invalid_argument);
}

std::string TemplateIdentifier::toString() const {
  std::string Result = TemplateUSR;
  if (!CanonicalArguments.empty()) {
    Result += "#" + CanonicalArguments;
  }
  if (!InstantiationContext.empty()) {
    Result += "@" + InstantiationContext;
  }
  Result += ":" + std::to_string(static_cast<int>(Kind));
  return Result;
}

bool TemplateInstantiationInfo::isValid() const {
  // Check if cache file exists and is newer than source
  if (CacheFile.empty() || SourceFile.empty())
    return false;

  // TODO: Add file timestamp validation
  return true;
}

//===----------------------------------------------------------------------===//
// TemplateUSRGenerator Implementation
//===----------------------------------------------------------------------===//

llvm::Expected<std::string> TemplateUSRGenerator::generateUSR(
    const ClassTemplateSpecializationDecl *Spec) {
  llvm::SmallString<128> Buf;
  if (index::generateUSRForDecl(Spec, Buf, Context.getLangOpts()))
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_usr,
        "Failed to generate USR for class template specialization");

  return std::string(Buf.str());
}

llvm::Expected<std::string> TemplateUSRGenerator::generateUSR(
    const FunctionDecl *FD,
    const FunctionTemplateSpecializationInfo *Spec) {
  llvm::SmallString<128> Buf;
  if (index::generateUSRForDecl(FD, Buf, Context.getLangOpts()))
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_usr,
        "Failed to generate USR for function template specialization");

  return std::string(Buf.str());
}

llvm::Expected<std::string> TemplateUSRGenerator::generateUSR(
    const VarTemplateSpecializationDecl *Spec) {
  llvm::SmallString<128> Buf;
  if (index::generateUSRForDecl(Spec, Buf, Context.getLangOpts()))
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_usr,
        "Failed to generate USR for variable template specialization");

  return std::string(Buf.str());
}

llvm::Expected<TemplateIdentifier> TemplateUSRGenerator::generateTemplateIdentifier(
    const TemplateDecl *Template,
    const TemplateArgumentList &Args,
    TemplateSpecializationKind Kind) {

  // Generate base template USR
  llvm::SmallString<128> Buf;
  if (index::generateUSRForDecl(Template, Buf, Context.getLangOpts()))
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::invalid_template_usr,
        "Failed to generate USR for template declaration");

  std::string TemplateUSR = std::string(Buf.str());
  std::string CanonicalArgs = canonicalizeTemplateArguments(Args);
  std::string InstContext = getInstantiationContext(Template);

  return TemplateIdentifier(TemplateUSR, CanonicalArgs, InstContext, Kind);
}

std::string TemplateUSRGenerator::canonicalizeTemplateArguments(
    const TemplateArgumentList &Args) {
  std::string Result;
  llvm::raw_string_ostream OS(Result);

  for (unsigned I = 0, E = Args.size(); I != E; ++I) {
    if (I > 0)
      OS << "#";

    const TemplateArgument &Arg = Args[I];
    switch (Arg.getKind()) {
    case TemplateArgument::Type: {
      llvm::SmallString<64> TypeBuf;
      if (!index::generateUSRForType(Arg.getAsType(), Context, TypeBuf,
                                   Context.getLangOpts())) {
        OS << TypeBuf;
      } else {
        OS << "T"; // Fallback for complex types
      }
      break;
    }
    case TemplateArgument::Integral:
      OS << "I" << Arg.getAsIntegral();
      break;
    case TemplateArgument::Declaration:
      if (const auto *ND = dyn_cast<NamedDecl>(Arg.getAsDecl())) {
        llvm::SmallString<64> DeclBuf;
        if (!index::generateUSRForDecl(ND, DeclBuf, Context.getLangOpts())) {
          OS << DeclBuf;
        } else {
          OS << "D"; // Fallback
        }
      }
      break;
    case TemplateArgument::Null:
      OS << "NULL";
      break;
    case TemplateArgument::NullPtr:
      OS << "N";
      break;
    case TemplateArgument::Template:
      // TODO: Handle template template arguments
      OS << "TT";
      break;
    case TemplateArgument::TemplateExpansion:
      OS << "TE";
      break;
    case TemplateArgument::Expression:
      OS << "E";
      break;
    case TemplateArgument::StructuralValue:
      OS << "SV";
      break;
    case TemplateArgument::Pack:
      OS << "P";
      for (const auto &PackArg : Arg.pack_elements()) {
        // Recursively handle pack elements
        OS << "_";
        (void)PackArg; // Suppress unused variable warning
      }
      break;
    }
  }

  return OS.str();
}

std::string TemplateUSRGenerator::getInstantiationContext(const Decl *D) {
  std::string Context;
  llvm::raw_string_ostream OS(Context);

  const DeclContext *DC = D->getDeclContext();
  while (DC && !DC->isTranslationUnit()) {
    if (const auto *ND = dyn_cast<NamedDecl>(DC)) {
      if (!Context.empty())
        OS << "::";
      OS << ND->getNameAsString();
    }
    DC = DC->getParent();
  }

  return OS.str();
}

//===----------------------------------------------------------------------===//
// TemplateASTUnitStorage Implementation
//===----------------------------------------------------------------------===//

/// Helper class for loading template AST units
class TemplateASTLoader {
public:
  TemplateASTLoader(CompilerInstance &CI, StringRef CTUDir)
      : CI(CI), CTUDir(CTUDir) {}

  llvm::Expected<std::unique_ptr<ASTUnit>> load(StringRef Identifier) {
    // For now, delegate to the existing CTU loading mechanism
    // TODO: Implement template-specific loading logic
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::template_not_found_in_cache,
        "Template AST loading not yet implemented");
  }

private:
  CompilerInstance &CI;
  StringRef CTUDir;
};

TemplateASTUnitStorage::TemplateASTUnitStorage(CompilerInstance &CI)
    : CI(CI), Loader(std::make_unique<TemplateASTLoader>(CI, "")) {}

TemplateASTUnitStorage::~TemplateASTUnitStorage() = default;

llvm::Expected<ASTUnit *> TemplateASTUnitStorage::getASTUnitForTemplate(
    const TemplateIdentifier &TID,
    StringRef CrossTUDir,
    StringRef IndexName,
    bool DisplayProgress) {

  std::string TIDStr = TID.toString();

  // Check if already loaded
  auto It = TemplateNameASTUnitMap.find(TIDStr);
  if (It != TemplateNameASTUnitMap.end())
    return It->second;

  // Load index if needed
  if (auto Err = ensureTemplateIndexLoaded(CrossTUDir, IndexName))
    return std::move(Err);

  // Find file for template
  auto FileIt = TemplateNameFileMap.find(TIDStr);
  if (FileIt == TemplateNameFileMap.end())
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::template_not_found_in_cache,
        "Template not found in index: " + TIDStr);

  return getASTUnitForFile(FileIt->second, DisplayProgress);
}

llvm::Expected<const Decl *> TemplateASTUnitStorage::getCachedTemplateInstantiation(
    const TemplateIdentifier &TID) {

  auto It = TemplateInstantiationSpecMap.find(TID);
  if (It == TemplateInstantiationSpecMap.end())
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::template_not_found_in_cache,
        "Template instantiation not found in cache");

  // TODO: Load and return the cached declaration
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::template_not_found_in_cache,
      "Template instantiation loading not yet implemented");
}

llvm::Error TemplateASTUnitStorage::cacheTemplateInstantiation(
    const TemplateIdentifier &TID,
    const Decl *InstantiatedDecl,
    ASTUnit *SourceUnit) {

  // TODO: Implement template instantiation caching
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::template_cache_write_failed,
      "Template instantiation caching not yet implemented");
}

llvm::Expected<std::string> TemplateASTUnitStorage::getFileForTemplate(
    const TemplateIdentifier &TID,
    StringRef CrossTUDir,
    StringRef IndexName) {

  if (auto Err = ensureTemplateIndexLoaded(CrossTUDir, IndexName))
    return std::move(Err);

  std::string TIDStr = TID.toString();
  auto It = TemplateNameFileMap.find(TIDStr);
  if (It == TemplateNameFileMap.end())
    return llvm::make_error<TemplateCacheError>(
        template_cache_error_code::template_not_found_in_cache,
        "Template not found in index: " + TIDStr);

  return It->second;
}

llvm::Error TemplateASTUnitStorage::ensureTemplateIndexLoaded(
    StringRef CrossTUDir, StringRef IndexName) {

  // TODO: Implement template index loading
  // For now, return success to allow compilation
  return llvm::Error::success();
}

llvm::Expected<ASTUnit *> TemplateASTUnitStorage::getASTUnitForFile(
    StringRef FileName, bool DisplayProgress) {

  // Check if already loaded
  auto It = TemplateFileASTUnitMap.find(FileName);
  if (It != TemplateFileASTUnitMap.end())
    return It->second.get();

  // Load the AST unit
  auto LoadResult = Loader->load(FileName);
  if (!LoadResult)
    return LoadResult.takeError();

  ASTUnit *Unit = LoadResult->get();
  TemplateFileASTUnitMap[FileName] = std::move(*LoadResult);

  return Unit;
}

//===----------------------------------------------------------------------===//
// TemplateInstantiationCache Implementation
//===----------------------------------------------------------------------===//

TemplateInstantiationCache::TemplateInstantiationCache(
    CompilerInstance &CI, CrossTranslationUnitContext &CTU)
    : CI(CI), CTUContext(CTU),
      USRGen(std::make_unique<TemplateUSRGenerator>(CI.getASTContext())),
      Storage(std::make_unique<TemplateASTUnitStorage>(CI)) {}

TemplateInstantiationCache::~TemplateInstantiationCache() = default;

llvm::Expected<const Decl *> TemplateInstantiationCache::getCachedTemplateInstantiation(
    const ClassTemplateSpecializationDecl *Spec,
    StringRef CrossTUDir,
    StringRef IndexName) {

  auto TIDResult = getTemplateIdentifier(Spec);
  if (!TIDResult)
    return TIDResult.takeError();

  return Storage->getCachedTemplateInstantiation(*TIDResult);
}

llvm::Expected<const Decl *> TemplateInstantiationCache::getCachedTemplateInstantiation(
    const FunctionDecl *FD,
    const FunctionTemplateSpecializationInfo *Spec,
    StringRef CrossTUDir,
    StringRef IndexName) {

  auto TIDResult = getTemplateIdentifier(FD);
  if (!TIDResult)
    return TIDResult.takeError();

  return Storage->getCachedTemplateInstantiation(*TIDResult);
}

llvm::Expected<const Decl *> TemplateInstantiationCache::getCachedTemplateInstantiation(
    const VarTemplateSpecializationDecl *Spec,
    StringRef CrossTUDir,
    StringRef IndexName) {

  auto TIDResult = getTemplateIdentifier(Spec);
  if (!TIDResult)
    return TIDResult.takeError();

  return Storage->getCachedTemplateInstantiation(*TIDResult);
}

llvm::Error TemplateInstantiationCache::cacheTemplateInstantiation(
    const Decl *InstantiatedDecl,
    const TemplateArgumentList &Args,
    StringRef CrossTUDir,
    StringRef IndexName) {

  auto TIDResult = getTemplateIdentifier(InstantiatedDecl);
  if (!TIDResult)
    return TIDResult.takeError();

  return Storage->cacheTemplateInstantiation(*TIDResult, InstantiatedDecl, nullptr);
}

llvm::Expected<bool> TemplateInstantiationCache::getCachedConstraintSatisfaction(
    const ConceptDecl *Concept,
    const TemplateArgumentList &Args,
    StringRef CrossTUDir,
    StringRef IndexName) {

  // TODO: Implement constraint satisfaction caching
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::constraint_evaluation_failed,
      "Constraint satisfaction caching not yet implemented");
}

llvm::Error TemplateInstantiationCache::cacheConstraintSatisfaction(
    const ConceptDecl *Concept,
    const TemplateArgumentList &Args,
    bool IsSatisfied,
    StringRef CrossTUDir,
    StringRef IndexName) {

  // TODO: Implement constraint satisfaction caching
  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::constraint_evaluation_failed,
      "Constraint satisfaction caching not yet implemented");
}

bool TemplateInstantiationCache::isTemplateCached(
    const TemplateIdentifier &TID,
    StringRef CrossTUDir,
    StringRef IndexName) {

  // TODO: Implement cache checking
  return false;
}

llvm::Error TemplateInstantiationCache::invalidateDependentCaches(
    StringRef HeaderPath,
    StringRef CrossTUDir,
    StringRef IndexName) {

  // TODO: Implement cache invalidation
  return llvm::Error::success();
}

llvm::Expected<TemplateIdentifier> TemplateInstantiationCache::getTemplateIdentifier(
    const Decl *D) {

  if (const auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
    return USRGen->generateTemplateIdentifier(
        CTSD->getSpecializedTemplate(),
        CTSD->getTemplateArgs(),
        CTSD->getSpecializationKind());
  }

  if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
    if (const auto *Spec = FD->getTemplateSpecializationInfo()) {
      return USRGen->generateTemplateIdentifier(
          Spec->getTemplate(),
          *Spec->TemplateArguments,
          Spec->getTemplateSpecializationKind());
    }
  }

  if (const auto *VTSD = dyn_cast<VarTemplateSpecializationDecl>(D)) {
    return USRGen->generateTemplateIdentifier(
        VTSD->getSpecializedTemplate(),
        VTSD->getTemplateArgs(),
        VTSD->getSpecializationKind());
  }

  return llvm::make_error<TemplateCacheError>(
      template_cache_error_code::invalid_template_usr,
      "Unsupported template declaration type");
}

//===----------------------------------------------------------------------===//
// Index Parsing Functions
//===----------------------------------------------------------------------===//

llvm::Expected<llvm::StringMap<std::string>>
parseTemplateCacheIndex(StringRef IndexPath) {
  std::ifstream ExternalFnMapFile{std::string(IndexPath)};
  if (!ExternalFnMapFile)
    return llvm::make_error<IndexError>(index_error_code::missing_index_file,
                                       std::string(IndexPath));

  llvm::StringMap<std::string> Result;
  std::string Line;
  unsigned LineNo = 1;

  while (std::getline(ExternalFnMapFile, Line)) {
    const size_t Pos = Line.find(" ");
    if (Pos > 0 && Pos != std::string::npos) {
      StringRef USR = StringRef(Line).substr(0, Pos);
      StringRef FilePath = StringRef(Line).substr(Pos + 1);
      Result[USR] = std::string(FilePath);
    } else if (!Line.empty()) {
      return llvm::make_error<IndexError>(
          index_error_code::invalid_index_format, std::string(IndexPath), LineNo);
    }
    ++LineNo;
  }

  return Result;
}

std::string createTemplateCacheIndexString(
    const llvm::StringMap<std::string> &Index) {
  std::ostringstream Result;
  for (const auto &Entry : Index) {
    Result << Entry.getKey().str() << " " << Entry.getValue() << "\n";
  }
  return Result.str();
}

} // namespace cross_tu
} // namespace clang