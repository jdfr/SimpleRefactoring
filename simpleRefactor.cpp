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

#define FUNCTION_NAMES "configOption", "configVariable", "config"

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

class RefactorEngine {
    Rewriter *Rewrite;
    SourceManager *SourceMgr;
    const SrcMgr::ContentCache * Content;
    StringRef fileBuffer;
    FileID FID;
    const FileEntry * fileEntry;
    bool computed;
    
    void recompute(SourceLocation loc) {
        FileID f(SourceMgr->getFileID(loc));
        if (!computed || f!=FID) {
            computed = true;
            FID = f;
            fileEntry = SourceMgr->getFileEntryForID(FID);
            Content = SourceMgr->getSLocEntry(FID).getFile().getContentCache();
            fileBuffer = SourceMgr->getBufferData(FID);
        }
    }
    
    //alternative implementation of SourceManager::getComposedLoc() (cannot backport because the original uses private methods), probably more expensive
    SourceLocation getComposedLoc(FileID FID, unsigned Offset) const {
        //return SourceMgr->getComposedLoc(FID, Offset);
        unsigned line = SourceMgr->getLineNumber(FID, Offset);
        unsigned column = SourceMgr->getColumnNumber(FID, Offset);
        return SourceMgr->translateFileLineCol(fileEntry, line, column);
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
        return SourceMgr->getLineNumber(FID, SourceMgr->getFileOffset(loc));
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
                Rewrite->RemoveText(getComposedLoc(FID, indentRange.end()-lenToRemove), lenToRemove);
            }
        }
        return ifIndentRange;
    }
    
public:
    RefactorEngine() : computed(false) {}

    void setRewriter(Rewriter *R) { Rewrite = R; SourceMgr = &R->getSourceMgr(); computed = false; }
    
    void showLine(const SourceLocation loc) {
        recompute(loc);
        llvm::errs() << "LINE: <" << getLine(loc) << " in " << FID.getHashValue() << ">\n";
    }

    void simpleRefactorIfStmt(const IfStmt *IfS, bool value) {
        //const Expr *cond = IfS->getCond();
        const Stmt *Then = IfS->getThen();
        const Stmt *Else = IfS->getElse();
        const Stmt *branch = value ? Then : Else;
        if (branch) {
            const Stmt &branchR = *branch;
            bool compound = isa<CompoundStmt>(branchR);
            //the if keyword's indent range might be handy later
            IndentRange ifIndentRange = reindentBranch(IfS, branch);
            if (value) {
                Rewrite->RemoveText(SourceRange(IfS->getIfLoc(), Then->getLocStart().getLocWithOffset(-1)));
                Rewrite->RemoveText(SourceRange(Then->getLocEnd().getLocWithOffset(1), IfS->getLocEnd()));
            } else {
                Rewrite->RemoveText(SourceRange(IfS->getIfLoc(), Else->getLocStart().getLocWithOffset(-1)));
            }
            if (compound) {
                const CompoundStmt &branchC = cast<CompoundStmt>(branchR);
                Rewrite->RemoveText(branchC.getLBracLoc(), 1);
                Rewrite->RemoveText(branchC.getRBracLoc(), 1);
            } else if (ifIndentRange.size>0){
                /* At least in branches of conditional statements, clang treats the whitespace before simple
                * statements as part of them, while compound statements just ignore whitespace. The unpleasant
                * side effect is that it is difficult to get the indentation of simple statements right
                * working purely from the AST, as simple statements have one indelible whitespace character.
                * This is a hack to get decent indentation for simple statements, most of the time, provided
                * there are no inconvenient tabs within the whitespace. */
                Rewrite->RemoveText(getComposedLoc(FID, ifIndentRange.end()-1), 1);
            }
        } else {
            Rewrite->RemoveText(IfS->getSourceRange());
        }
    }

    void simpleReplaceExpr(const Stmt *expr, StringRef value) {
        Rewrite->ReplaceText(expr->getSourceRange(), value);
    }

    void simpleReplaceExpr(const Stmt *expr, const Stmt *newexpr) {
        Rewrite->ReplaceText(expr->getSourceRange(), newexpr->getSourceRange());
    }

};

//Result type for MatchHandler::simplePartialEvaluation
typedef struct CondResult {
    bool replaceByBool, val;
    const Stmt *sub, *newval;
    CondResult(const Stmt *s, bool v)       : replaceByBool(true),  val(v), sub(s) {}
    CondResult(const Stmt *s, const Stmt *n): replaceByBool(false), sub(s), newval(n) {}
} CondResult;

class MatchHandler : public MatchFinder::MatchCallback {
public:
    MatchHandler(RefactorEngine *r, std::string _literal) : refactorTool(r), literal(_literal) {}

    void setContext(ASTContext *c) { context = c; }

    virtual void run(const MatchFinder::MatchResult &Result) {
        ASTContext *Context = Result.Context;
        const StringLiteral *lit = Result.Nodes.getNodeAs<clang::StringLiteral>("strLiteral");
        bool isLit = lit && lit->getString().str() == literal;
        if (!isLit) return;

        const IfStmt              *IfS = Result.Nodes.getNodeAs<clang::IfStmt>             ("ifStmtWithConfigInCond");
        const ConditionalOperator *CdE = Result.Nodes.getNodeAs<clang::ConditionalOperator>("condOpWithConfigInCond");

        if (IfS && CdE) {
            llvm::errs() << "This tool expects the matchers to be applied sequentially!!!!\n";
            return;
        } else if (IfS || CdE) {
            /*if (IfS) {
                llvm::errs() << "IF STATEMENT WITH CONFIG VALUE, "; refactorTool->showLine(IfS->getIfLoc());
                IfS->dump();
            } else {
                llvm::errs() << "CONDITIONAL OPERATOR WITH CONFIG VALUE, "; refactorTool->showLine(CdE->getLocStart());
                CdE->dump();
            }*/
            const CallExpr *config = Result.Nodes.getNodeAs<CallExpr>("callToConfigFunction");
            const Expr *cond = IfS ? IfS->getCond() : CdE->getCond();
            CondResult res = simplePartialEvaluation(config, TermValue.getValue(), cond);
            if (res.replaceByBool) {
                if (res.sub == cond) {
                    if (IfS) {
                        refactorTool->simpleRefactorIfStmt(IfS, res.val);
                    } else {
                        //because of the very low precedence of the conditional operator, it is mostly safe to discard possible parentheses here
                        refactorTool->simpleReplaceExpr(CdE, getExprIgnoreParensAndImpCasts(res.val ? CdE->getLHS() : CdE->getRHS()));
                    }
                } else {
                    refactorTool->simpleReplaceExpr(res.sub, res.val ? "true" : "false");
                }
            } else {
                //TODO: parentheses might be necessary after this rewriting; for example: UNRELATEDCONDITION && CONFIG && (A || B).
                //      Ideally, we should add/remove them as necessary, taking into account the relative precedences of the ancestor and children
                //      Meanwhile, this is a crude approximation to remove them if it seems safe to do so
                const Expr *condNoParens = getExprIgnoreParensAndImpCasts(cond);
                if (cast<Expr>(res.sub)==condNoParens) {
                    res.sub = cond;
                    res.newval = cast<Stmt>(getExprIgnoreParensAndImpCasts(cast<Expr>(res.newval)));
                }
                refactorTool->simpleReplaceExpr(res.sub, res.newval);
            }
            return;
        }

    }

    const Expr *getExprIgnoreParensAndImpCasts(const Expr *expr) {
        while (isa<ParenExpr>(expr) || isa<CastExpr>(expr) || isa<ExprWithCleanups>(expr)) {
            while (isa<ParenExpr>(expr)) {
                expr = cast<ParenExpr>(expr)->getSubExpr();
            }
            while (isa<ImplicitCastExpr>(expr)) {
                expr = cast<ImplicitCastExpr>(expr)->getSubExpr();
            }
            while (isa<ExprWithCleanups>(expr)) {
                expr = cast<ExprWithCleanups>(expr)->getSubExpr();
            }
        }
        return expr;
    }

    CondResult simplePartialEvaluation(const Expr *sub, bool value, const Expr *wholeCond) {
        const Stmt *s = sub, *toReturn = sub, *parent;
        bool keepSearching = true;
        while(s != wholeCond && keepSearching) {
            const auto& allparents = context->getParents(*s);
            if (allparents.size()!=1) {
                llvm::errs() << "expression ancestry different than expected! At one point it has " << allparents.size() << " simultaneous ancestors!!!\n Bailing out of unwinding process...";
                showParents(s);
                keepSearching = false;
            } else {
                parent = allparents[0].get<Stmt>();
                if (isa<ParenExpr>(parent)        ||
                    isa<CastExpr>(parent)         ||
                    isa<ExprWithCleanups>(parent) ) {
                    //just go up!
                } else if (isa<UnaryOperator>(parent)) {
                    const UnaryOperator *o = allparents[0].get<UnaryOperator>();
                    if (o->getOpcode()==UO_LNot) {
                        value = !value;
                    } else {
                        //we do not know how to handle this, so bailing out.
                        keepSearching = false;
                    }
                } else if (isa<BinaryOperator>(parent)) {
                    const BinaryOperator *o = allparents[0].get<BinaryOperator>();
                    auto opcode = o->getOpcode();
                    if (opcode!=BO_LAnd && opcode!=BO_LOr) {
                        //we do not know how to handle this, so bailing out.
                        keepSearching = false;
                    } else if ((opcode==BO_LAnd && !value) || (opcode==BO_LOr && value)) {
                        //just go up!
                    } else if ((opcode==BO_LAnd && value) || (opcode==BO_LOr && !value)) {
                        //ok, we stop at this point and rewrite just a section of the logical expression
                        const Stmt *lhs = o->getLHS();
                        const Stmt *rhs = o->getRHS();
                        const Stmt *other;
                        if (lhs==s) {
                            return CondResult(o, rhs);
                        } else if (rhs==s) {
                            return CondResult(o, lhs);
                        } else {
                            llvm::errs() << "There's something puzzling in the tree of this binary operator, so bailing out!\n";
                            o->dump();
                            keepSearching = false;
                        }
                    } else {
                        //we do not know how to handle this, so bailing out.
                        keepSearching = false;
                    }
                }
            }
            if (keepSearching) {
                s = parent;
            }
        }
        return CondResult(s, value);
    }

    void showParents(const Stmt *s) {
        const auto& parents = context->getParents(*s);
        int x=0;
        for (auto parent: parents) {
            llvm::errs() << "  Parent " << x++ << ": \n";
            parent.get<Stmt>()->dump();
        }
    }


private:
    RefactorEngine *refactorTool;
    std::string literal;
    ASTContext *context;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser. It registers a couple of matchers and runs them on
// the AST.
class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(RefactorEngine *R) : handler(R, TermName.getValue()) {
        // Add a simple matcher for finding 'if' statements.

        auto functionCall =
            callExpr(
                callee(functionDecl(hasAnyName(FUNCTION_NAMES))),
                hasArgument(0, allOf(
                                    hasDescendant(stringLiteral().bind("strLiteral")),
                                    unless(anyOf(
                                        hasDescendant(callExpr()),
                                        hasDescendant(binaryOperator()),
                                        hasDescendant(unaryOperator())
                                    ))
                ))
                ).bind("callToConfigFunction");

        //TODO: REWRITE ALL THIS SO WE EXCLUSIVELY MATCH THE CONFIG FUNCTION CALLS!!!!!
        //      Specifically, this will require substantital refactoring to check if the calls are enclosed in the condition of an IfStmt or ConditionalOperator
        //REASON: What if a matching conditional operator is nested in the condition of an also matching "if" statement/conditional operator?
        //        We will not be amused debugging the ensuing chaos...
        //        And matchers have not the right level of abstraction to discard such convoluted cases...

        Matcher.addMatcher(
            ifStmt(
                hasCondition(forEachDescendant(functionCall))
            ).bind("ifStmtWithConfigInCond"),
            &handler
        );

        Matcher.addMatcher(
            conditionalOperator(
                hasCondition(forEachDescendant(functionCall))
            ).bind("condOpWithConfigInCond"),
            &handler
        );
    }

  void HandleTranslationUnit(ASTContext &Context) override {
    handler.setContext(&Context);
    // Run the matchers when we have the whole TU parsed.
    Matcher.matchAST(Context);
  }

private:
  MatchHandler handler;
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
    refactorTool.setRewriter(&TheRewriter);
    return llvm::make_unique<MyASTConsumer>(&refactorTool);
  }

private:
  Rewriter TheRewriter;
  RefactorEngine refactorTool;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, CustomOptions);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
