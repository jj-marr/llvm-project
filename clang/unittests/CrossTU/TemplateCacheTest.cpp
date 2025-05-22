//===--- TemplateCacheTest.cpp - Template Cache Unit Tests -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/CrossTU/TemplateCache.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"
#include <memory>

using namespace clang;
using namespace clang::cross_tu;
using namespace llvm;

namespace {

class TemplateCacheTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a temporary directory for cache files
    std::error_code EC = llvm::sys::fs::createUniqueDirectory("template-cache-test", TempDir);
    ASSERT_FALSE(EC) << "Failed to create temp directory: " << EC.message();
  }

  void TearDown() override {
    // Clean up temporary directory
    if (!TempDir.empty()) {
      llvm::sys::fs::remove_directories(TempDir);
    }
  }

  std::unique_ptr<ASTUnit> createASTUnit(StringRef Code) {
    return tooling::buildASTFromCode(Code, "test.cpp");
  }

  SmallString<256> TempDir;
};

// Test TemplateIdentifier functionality
TEST_F(TemplateCacheTest, TemplateIdentifierBasic) {
  TemplateIdentifier TID1("template_usr", "int,double", "context1", TSK_ImplicitInstantiation);
  TemplateIdentifier TID2("template_usr", "int,double", "context1", TSK_ImplicitInstantiation);
  TemplateIdentifier TID3("template_usr", "int,float", "context1", TSK_ImplicitInstantiation);

  // Test equality
  EXPECT_EQ(TID1, TID2);
  EXPECT_NE(TID1, TID3);

  // Test ordering
  EXPECT_LT(TID3, TID1); // float < double lexicographically

  // Test string representation
  std::string TIDStr = TID1.toString();
  EXPECT_FALSE(TIDStr.empty());
  EXPECT_NE(TIDStr.find("template_usr"), std::string::npos);
}

TEST_F(TemplateCacheTest, TemplateIdentifierHash) {
  TemplateIdentifier TID1("template_usr", "int,double", "context1", TSK_ImplicitInstantiation);
  TemplateIdentifier TID2("template_usr", "int,double", "context1", TSK_ImplicitInstantiation);
  TemplateIdentifier TID3("template_usr", "int,float", "context1", TSK_ImplicitInstantiation);

  // Test hash consistency
  auto Hash1 = DenseMapInfo<TemplateIdentifier>::getHashValue(TID1);
  auto Hash2 = DenseMapInfo<TemplateIdentifier>::getHashValue(TID2);
  auto Hash3 = DenseMapInfo<TemplateIdentifier>::getHashValue(TID3);

  EXPECT_EQ(Hash1, Hash2);
  EXPECT_NE(Hash1, Hash3);

  // Test special keys
  auto EmptyKey = DenseMapInfo<TemplateIdentifier>::getEmptyKey();
  auto TombstoneKey = DenseMapInfo<TemplateIdentifier>::getTombstoneKey();
  EXPECT_NE(EmptyKey, TombstoneKey);
  EXPECT_TRUE(DenseMapInfo<TemplateIdentifier>::isEqual(EmptyKey, EmptyKey));
}

// Test TemplateInstantiationInfo functionality
TEST_F(TemplateCacheTest, TemplateInstantiationInfo) {
  SourceLocation POI; // Invalid location for test
  TemplateInstantiationInfo Info(POI, TSK_ImplicitInstantiation,
                                "source.cpp", "cache.ast", true);

  EXPECT_EQ(Info.SpecKind, TSK_ImplicitInstantiation);
  EXPECT_EQ(Info.SourceFile, "source.cpp");
  EXPECT_EQ(Info.CacheFile, "cache.ast");
  EXPECT_TRUE(Info.IsConstraintSatisfied);
  EXPECT_TRUE(Info.isValid());
}

// Test TemplateUSRGenerator functionality
TEST_F(TemplateCacheTest, TemplateUSRGenerator) {
  const char *Code = R"(
    template<typename T, int N>
    class TestTemplate {
    public:
      T data[N];
    };

    template<typename T>
    void testFunction(T value) {}

    template<typename T>
    T testVariable = T{};

    // Instantiations
    TestTemplate<int, 5> instance1;
    void test() {
      testFunction<double>(3.14);
      auto& var = testVariable<float>;
    }
  )";

  auto AST = createASTUnit(Code);
  ASSERT_TRUE(AST);

  TemplateUSRGenerator USRGen(AST->getASTContext());

  // Find template specializations in the AST
  auto &Context = AST->getASTContext();
  auto *TU = Context.getTranslationUnitDecl();

  bool FoundClassSpec = false;
  bool FoundFunctionSpec = false;

  for (auto *D : TU->decls()) {
    if (auto *VD = dyn_cast<VarDecl>(D)) {
      if (VD->getName() == "instance1") {
        auto *ClassSpec = dyn_cast<ClassTemplateSpecializationDecl>(
            VD->getType()->getAsCXXRecordDecl());
        if (ClassSpec) {
          auto USRResult = USRGen.generateUSR(ClassSpec);
          EXPECT_THAT_EXPECTED(USRResult, Succeeded());
          if (USRResult) {
            EXPECT_FALSE(USRResult->empty());
            FoundClassSpec = true;
          }
        }
      }
    }
  }

  EXPECT_TRUE(FoundClassSpec);
}

// Test TemplateCacheError functionality
TEST_F(TemplateCacheTest, TemplateCacheError) {
  TemplateCacheError Error1(template_cache_error_code::invalid_template_usr);
  EXPECT_EQ(Error1.getCode(), template_cache_error_code::invalid_template_usr);
  EXPECT_TRUE(Error1.getMessage().empty());

  TemplateCacheError Error2(template_cache_error_code::template_instantiation_failed,
                           "Custom error message");
  EXPECT_EQ(Error2.getCode(), template_cache_error_code::template_instantiation_failed);
  EXPECT_EQ(Error2.getMessage(), "Custom error message");

  // Test error code conversion
  auto EC = Error1.convertToErrorCode();
  EXPECT_TRUE(EC);
}

// Test template cache index parsing
TEST_F(TemplateCacheTest, TemplateCacheIndexParsing) {
  // Create a temporary index file
  SmallString<256> IndexPath = TempDir;
  llvm::sys::path::append(IndexPath, "template_index.txt");

  std::string IndexContent = R"(template_usr1 /path/to/file1.ast
template_usr2 /path/to/file2.ast
template_usr3 /path/to/file3.ast
)";

  std::error_code EC;
  llvm::raw_fd_ostream OS(IndexPath, EC);
  ASSERT_FALSE(EC);
  OS << IndexContent;
  OS.close();

  // Test parsing
  auto ParseResult = parseTemplateCacheIndex(IndexPath);
  EXPECT_THAT_EXPECTED(ParseResult, Succeeded());

  if (ParseResult) {
    auto &Index = *ParseResult;
    EXPECT_EQ(Index.size(), 3u);
    EXPECT_EQ(Index["template_usr1"], "/path/to/file1.ast");
    EXPECT_EQ(Index["template_usr2"], "/path/to/file2.ast");
    EXPECT_EQ(Index["template_usr3"], "/path/to/file3.ast");

    // Test index string creation
    std::string IndexStr = createTemplateCacheIndexString(Index);
    EXPECT_FALSE(IndexStr.empty());
    EXPECT_NE(IndexStr.find("template_usr1"), std::string::npos);
    EXPECT_NE(IndexStr.find("/path/to/file1.ast"), std::string::npos);
  }
}

// Test invalid index file handling
TEST_F(TemplateCacheTest, InvalidTemplateCacheIndex) {
  SmallString<256> InvalidIndexPath = TempDir;
  llvm::sys::path::append(InvalidIndexPath, "invalid_index.txt");

  // Test non-existent file
  auto ParseResult = parseTemplateCacheIndex(InvalidIndexPath);
  EXPECT_THAT_EXPECTED(ParseResult, Failed());

  // Create malformed index file
  std::error_code EC;
  llvm::raw_fd_ostream OS(InvalidIndexPath, EC);
  ASSERT_FALSE(EC);
  OS << "malformed line without space\n";
  OS << "another malformed line\n";
  OS.close();

  ParseResult = parseTemplateCacheIndex(InvalidIndexPath);
  // Should still succeed but with empty or partial results
  EXPECT_THAT_EXPECTED(ParseResult, Succeeded());
}

// Test TemplateASTUnitStorage would require CompilerInstance setup
// This test is covered by integration tests instead

// Test template cache statistics and performance metrics
TEST_F(TemplateCacheTest, TemplateCacheStatistics) {
  // This test would verify cache hit/miss statistics
  // For now, we test the basic structure

  TemplateIdentifier TID1("template1", "int", "", TSK_ImplicitInstantiation);
  TemplateIdentifier TID2("template2", "double", "", TSK_ImplicitInstantiation);

  // Test that different templates have different identifiers
  EXPECT_NE(TID1, TID2);

  // Test hash distribution
  auto Hash1 = DenseMapInfo<TemplateIdentifier>::getHashValue(TID1);
  auto Hash2 = DenseMapInfo<TemplateIdentifier>::getHashValue(TID2);
  EXPECT_NE(Hash1, Hash2);
}

// Test template specialization kinds
TEST_F(TemplateCacheTest, TemplateSpecializationKinds) {
  TemplateIdentifier Implicit("template", "int", "", TSK_ImplicitInstantiation);
  TemplateIdentifier Explicit("template", "int", "", TSK_ExplicitInstantiationDefinition);
  TemplateIdentifier Specialization("template", "int", "", TSK_ExplicitSpecialization);

  EXPECT_NE(Implicit, Explicit);
  EXPECT_NE(Explicit, Specialization);
  EXPECT_NE(Implicit, Specialization);

  // Test ordering
  EXPECT_LT(Implicit.Kind, Explicit.Kind);
}

// Test template context handling
TEST_F(TemplateCacheTest, TemplateContextHandling) {
  TemplateIdentifier NoContext("template", "int", "", TSK_ImplicitInstantiation);
  TemplateIdentifier WithContext("template", "int", "outer::inner", TSK_ImplicitInstantiation);

  EXPECT_NE(NoContext, WithContext);
  EXPECT_LT(NoContext, WithContext); // Empty context comes first
}

// Test error handling for template cache operations
TEST_F(TemplateCacheTest, ErrorHandling) {
  // Test various error conditions
  TemplateCacheError InvalidUSR(template_cache_error_code::invalid_template_usr,
                               "Invalid USR format");
  TemplateCacheError InstantiationFailed(template_cache_error_code::template_instantiation_failed,
                                        "Template instantiation failed");
  TemplateCacheError CacheCorrupted(template_cache_error_code::template_cache_corrupted,
                                   "Cache file is corrupted");

  EXPECT_EQ(InvalidUSR.getCode(), template_cache_error_code::invalid_template_usr);
  EXPECT_EQ(InstantiationFailed.getCode(), template_cache_error_code::template_instantiation_failed);
  EXPECT_EQ(CacheCorrupted.getCode(), template_cache_error_code::template_cache_corrupted);

  // Test error messages
  EXPECT_EQ(InvalidUSR.getMessage(), "Invalid USR format");
  EXPECT_EQ(InstantiationFailed.getMessage(), "Template instantiation failed");
  EXPECT_EQ(CacheCorrupted.getMessage(), "Cache file is corrupted");
}

// Test template dependency tracking
TEST_F(TemplateCacheTest, TemplateDependencyTracking) {
  TemplateInstantiationInfo Info;
  Info.DependentHeaders.push_back("header1.h");
  Info.DependentHeaders.push_back("header2.h");
  Info.DependentHeaders.push_back("header3.h");

  EXPECT_EQ(Info.DependentHeaders.size(), 3u);
  EXPECT_EQ(Info.DependentHeaders[0], "header1.h");
  EXPECT_EQ(Info.DependentHeaders[1], "header2.h");
  EXPECT_EQ(Info.DependentHeaders[2], "header3.h");
}

// Test cache time tracking
TEST_F(TemplateCacheTest, CacheTimeTracking) {
  auto StartTime = std::chrono::system_clock::now();

  TemplateInstantiationInfo Info(SourceLocation(), TSK_ImplicitInstantiation,
                                "source.cpp", "cache.ast", true);

  auto EndTime = std::chrono::system_clock::now();

  // Cache time should be between start and end time
  EXPECT_GE(Info.CacheTime, StartTime);
  EXPECT_LE(Info.CacheTime, EndTime);
}

} // anonymous namespace