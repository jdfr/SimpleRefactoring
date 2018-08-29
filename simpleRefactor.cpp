//------------------------------------------------------------------------------
// Adapted from Eli Bendersky's sample code
//------------------------------------------------------------------------------
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory CustomOptions("Custom options"); 
static llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage); 
static llvm::cl::opt<std::string> TermName("term", llvm::cl::cat(CustomOptions), llvm::cl::desc("config option name"), llvm::cl::value_desc("string literal (no spaces)")); 
static llvm::cl::opt<bool> TermValue("value", llvm::cl::cat(CustomOptions), llvm::cl::desc("config option value"), llvm::cl::value_desc("true/false")); 
static llvm::cl::opt<bool> Overwrite("overwrite", llvm::cl::cat(CustomOptions), llvm::cl::desc("overwrite source files"), llvm::cl::value_desc("true/false")); 

//this function was lifted wholesale from clang
static inline bool isWhitespaceExceptNL(unsigned char c) {
    switch (c) {
        case ' ': case '\t': case '\f': case '\v': case '\r':
            return true;
        default:
            return false;
    }
}
 
class IndentRange { 
public: 
    unsigned start, size;
    unsigned end() { return start+size-1; } 
    IndentRange(unsigned a, unsigned b) : start(a), size(b) {} 
};

class RefactorIfStmt {
    Rewriter &Rewrite;
    SourceManager &SourceMgr;
    const SrcMgr::ContentCache * Content;
    StringRef fileBuffer;
    FileID FID;
    const FileEntry * fileEntry;
    bool computed;
    
    void recompute(SourceLocation loc) {
        FileID f(SourceMgr.getFileID(loc));
        if (!computed || f!=FID) {
            computed = true;
            FID = f;
            fileEntry = SourceMgr.getFileEntryForID(FID);
            Content = SourceMgr.getSLocEntry(FID).getFile().getContentCache();
            fileBuffer = SourceMgr.getBufferData(FID);
        }
    }
    
    //alternative implementation of SourceManager::getComposedLoc() (cannot backport because the original uses private methods), probably more expensive
    SourceLocation getComposedLoc(FileID FID, unsigned Offset) const {
        //return SourceMgr.getComposedLoc(FID, Offset);
        unsigned line = SourceMgr.getLineNumber(FID, Offset);
        unsigned column = SourceMgr.getColumnNumber(FID, Offset);
        return SourceMgr.translateFileLineCol(fileEntry, line, column);
    }
  
    IndentRange getIndentRange(unsigned lineNumber) {
        unsigned lineOffs = Content->SourceLineCache[lineNumber-1];
        unsigned i = lineOffs;
        while (isWhitespaceExceptNL(fileBuffer[i])) ++i;
        return IndentRange(lineOffs, i-lineOffs);
    }
    
    StringRef space(IndentRange i) {
        return fileBuffer.substr(i.start, i.size);
    }

    unsigned getLine(SourceLocation loc) {
        return SourceMgr.getLineNumber(FID, SourceMgr.getFileOffset(loc));
    }
    
    IndentRange reindentBranch(const IfStmt *IfS, const Stmt *branch) {
        SourceLocation ifLoc = IfS->getIfLoc();
        recompute(ifLoc);
        unsigned ifLine = getLine(ifLoc);
        IndentRange ifIndentRange = getIndentRange(ifLine);
        int lenIfIndent = ifIndentRange.size;
        unsigned lineA = getLine(branch->getLocStart());
        unsigned lineB = getLine(branch->getLocEnd());
        for (unsigned line = lineA; line <= lineB; ++line) {
            IndentRange indentRange = getIndentRange(line);
            int lenIndent = indentRange.size;
            int lenToRemove = lenIndent-lenIfIndent;
            if (lenToRemove>0) {
                Rewrite.RemoveText(getComposedLoc(FID, indentRange.end()-lenToRemove), lenToRemove);
            }
        }
        return ifIndentRange;
    }
    
public:
    RefactorIfStmt(Rewriter &R) : Rewrite(R), SourceMgr(R.getSourceMgr()), computed(false) {}
    
    void simpleRefactoIfStmt(const IfStmt *IfS) {
        //const Expr *cond = IfS->getCond();
        const Stmt *Then = IfS->getThen();
        const Stmt *Else = IfS->getElse();
        const Stmt *branch = TermValue.getValue() ? Then : Else;
        if (branch) {
            const Stmt &branchR = *branch;
            bool compound = isa<CompoundStmt>(branchR);
            //the if keyword's indent range might be handy later
            IndentRange ifIndentRange = reindentBranch(IfS, branch);
            if (TermValue.getValue()) {
                Rewrite.RemoveText(SourceRange(IfS->getIfLoc(), Then->getLocStart().getLocWithOffset(-1))); 
                Rewrite.RemoveText(SourceRange(Then->getLocEnd().getLocWithOffset(1), IfS->getLocEnd()));
            } else {
                Rewrite.RemoveText(SourceRange(IfS->getIfLoc(), Else->getLocStart().getLocWithOffset(-1)));
            }
            if (compound) {
                const CompoundStmt &branchC = cast<CompoundStmt>(branchR);
                Rewrite.RemoveText(branchC.getLBracLoc(), 1);
                Rewrite.RemoveText(branchC.getRBracLoc(), 1);
            } else if (ifIndentRange.size>0){
                /* At least in branches of conditional statements, clang treats the whitespace before simple
                * statements as part of them, while compound statements just ignore whitespace. The unpleasant
                * side effect is that it is difficult to get the indentation of simple statements right
                * working purely from the AST, as simple statements have one indelible whitespace character.
                * This is a hack to get decent indentation for simple statements, most of the time, provided
                * there are no inconvenient tabs within the whitespace. */
                Rewrite.RemoveText(getComposedLoc(FID, ifIndentRange.end()-1), 1);
            }
        } else {
            Rewrite.RemoveText(IfS->getSourceRange());  
        }
    }

};


class IfStmtHandler : public MatchFinder::MatchCallback {
public:
  IfStmtHandler(Rewriter &_Rewrite, std::string _literal) : Rewrite(_Rewrite), reindentTool(_Rewrite), literal(_literal) {}

  virtual void run(const MatchFinder::MatchResult &Result) {
        // The matched 'if' statement was bound to 'ifStmt'.
        const IfStmt *IfS = Result.Nodes.getNodeAs<clang::IfStmt>("ifStmt");
        const StringLiteral *lit = Result.Nodes.getNodeAs<clang::StringLiteral>("strLiteral");
        if (IfS && lit && lit->getString().str() == literal) {
            reindentTool.simpleRefactoIfStmt(IfS);
        }
  }

private:
    Rewriter &Rewrite;
    RefactorIfStmt reindentTool;
    std::string literal;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(Rewriter &R) : HandlerForIf(R, TermName.getValue()) {
        // Add a simple matcher for finding 'if' statements.

        Matcher.addMatcher(
            ifStmt(
                hasCondition(has(ignoringParenImpCasts(
                    callExpr(
                        callee(functionDecl(hasAnyName("configOption", "configVariable", "config"))),
                        hasArgument(0, allOf(
                                            hasDescendant(stringLiteral().bind("strLiteral")),
                                            unless(anyOf(
                                                hasDescendant(callExpr()),
                                                hasDescendant(binaryOperator()),
                                                hasDescendant(unaryOperator())
                                            ))
                        ))
                     )//.bind("callToConfigFunction")
                )))
            ).bind("ifStmt"), 
            &HandlerForIf
        );
    }

  void HandleTranslationUnit(ASTContext &Context) override {
    // Run the matchers when we have the whole TU parsed.
    Matcher.matchAST(Context);
  }

private:
  IfStmtHandler HandlerForIf;
  MatchFinder Matcher;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  ~MyFrontendAction() {
      if (Overwrite.getValue()) {
          //this overwrite changes to source files, both the source file and the included headers
          llvm::errs() << "Overwriting files...\n";
          TheRewriter.overwriteChangedFiles();
          llvm::errs() << "Overwrite complete.\n";
      }
  }
  void EndSourceFileAction() override {
      if (!Overwrite.getValue()) {
        TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
      }
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }

private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, CustomOptions);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
