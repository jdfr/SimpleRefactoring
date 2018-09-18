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

//Result type for MatchHandler::getUseCase
enum ParentType {ParentUnknown, ParentIf, ParentCE, ParentAssignmentRHS, ParentVarDecl, ParentNonSpecial};
typedef struct ParentUseCase {
    ParentType type;
    const Stmt *parent;
    const Expr *cond;
    ParentUseCase() : type(ParentUnknown), parent(NULL), cond(NULL) {}
    ParentUseCase(ParentType t, const Stmt *s) : type(t), parent(s), cond(NULL) {}
    ParentUseCase(ParentType t, const Stmt *s, const Expr *e) : type(t), parent(s), cond(e) {}
} ParentUseCase;

class MatchHandler : public MatchFinder::MatchCallback {
public:
    MatchHandler(RefactorEngine *r, std::string _literal) : refactorTool(r), literal(_literal) {}

    void setContext(ASTContext *c) { context = c; }

    virtual void run(const MatchFinder::MatchResult &Result) {
        ASTContext *Context = Result.Context;
        const StringLiteral *lit = Result.Nodes.getNodeAs<clang::StringLiteral>("strLiteral");
        bool isLit = lit && lit->getString().str() == literal;
        if (!isLit) return;

        const CallExpr *config = Result.Nodes.getNodeAs<CallExpr>("callToConfigFunction");
        ParentUseCase p = getUseCase(cast<Stmt>(config));

        if (p.type==ParentIf || p.type==ParentCE) {
            CondResult res = simplePartialEvaluation(config, TermValue.getValue(), p.cond);
            if (res.replaceByBool) {
                if (res.sub == p.cond) {
                    if (p.type==ParentIf) {
                        refactorTool->simpleRefactorIfStmt(cast<IfStmt>(p.parent), res.val);
                    } else {
                        //because of the very low precedence of the conditional operator, it is mostly safe to discard possible parentheses here
                        const ConditionalOperator * CdE = cast<ConditionalOperator>(p.parent);
                        refactorTool->simpleReplaceExpr(p.parent, getExprIgnoreParensAndImpCasts(res.val ? CdE->getLHS() : CdE->getRHS()));
                    }
                } else {
                    refactorTool->simpleReplaceExpr(res.sub, res.val ? "true" : "false");
                }
            } else {
                //TODO: parentheses might be necessary after this rewriting; for example: UNRELATEDCONDITION && CONFIG && (A || B).
                //      Ideally, we should add/remove them as necessary, taking into account the relative precedences of the ancestor and children
                //      Meanwhile, this is a crude approximation to remove them if it seems safe to do so
                const Expr *condNoParens = getExprIgnoreParensAndImpCasts(p.cond);
                if (cast<Expr>(res.sub)==condNoParens) {
                    res.sub = p.cond;
                    res.newval = cast<Stmt>(getExprIgnoreParensAndImpCasts(cast<Expr>(res.newval)));
                }
                refactorTool->simpleReplaceExpr(res.sub, res.newval);
            }
        } else if (p.type==ParentAssignmentRHS || (p.type==ParentNonSpecial && isa<Expr>(p.parent))) {
            //TODO: when p.type==ParentAssignmentRHS, instead of this crude replacement, do further processing (out of the scope of this function):
            //      if the variable is not used before this assignment, and there are no side effects, it can be removed, and its instances replaced by its value
            //      (of course, the variable being assigned has to be captured in getUseCase()
            const Expr * ep = cast<Expr>(p.parent);
            CondResult res = simplePartialEvaluation(config, TermValue.getValue(), ep);
            if (res.replaceByBool) {
                refactorTool->simpleReplaceExpr(res.sub, res.val ? "true" : "false");
            } else {
                //TODO: same as above pattern
                const Expr *condNoParens = getExprIgnoreParensAndImpCasts(ep);
                if (cast<Expr>(res.sub)==condNoParens) {
                    res.sub = ep;
                    res.newval = cast<Stmt>(getExprIgnoreParensAndImpCasts(cast<Expr>(res.newval)));
                }
                refactorTool->simpleReplaceExpr(res.sub, res.newval);
            }
        }
    }

    //crude function to discriminate different use cases. For each one we recognize, we try to provide the best superexpression containing
    //the call to the config function, such that rewriting can be most optimized (i.e. redundant parentheses can be automatically discarded)
    ParentUseCase getUseCase(const Stmt *expr) {
        const Stmt *e = expr;
        //TODO: detect instances used in declarations / assignments to boolean variables/members.
        //      If doing so, further processing (out of the scope of this function): 
        while (!isa<CompoundStmt>(e)) {
            const auto& allparents = context->getParents(*e);
            if (allparents.size()==1) {
                if (allparents[0].get<Stmt>()!=NULL) {
                    const Stmt *parent = allparents[0].get<Stmt>();
                    if (isa<IfStmt>(parent)) {
                        const IfStmt *p = cast<IfStmt>(parent);
                        if (p->getCond()==e) {
                            return ParentUseCase(ParentIf, parent, p->getCond());
                        } else {
                            //not in the condition
                            return ParentUseCase(ParentNonSpecial, parent);
                        }
                    }
                    if (isa<ConditionalOperator>(parent)) {
                        const ConditionalOperator *p = cast<ConditionalOperator>(parent);
                        if (p->getCond()==e) {
                            return ParentUseCase(ParentCE, parent, p->getCond());
                        } else {
                            //not in the condition
                            return ParentUseCase(ParentNonSpecial, parent);
                        }
                    }
                    if (isa<BinaryOperator>(parent)) {
                        const BinaryOperator *p = cast<BinaryOperator>(parent);
                        if (p->getOpcode()==BO_Assign && p->getRHS()==e) {
                            //in an asignment
                            return ParentUseCase(ParentAssignmentRHS, p->getRHS());
                        }
                    }
                    if (isa<CallExpr>(parent)) {
                        const CallExpr *ce = cast<CallExpr>(parent);
                        int na = ce->getNumArgs();
                        for(int i=0; i<na; ++i) {
                            const Expr *arg = ce->getArg(i);
                            if (arg==e) {
                                return ParentUseCase(ParentNonSpecial, e);
                            }
                        }
                    }
                    if (isa<ReturnStmt>(parent)) {
                        //TODO: if the function has no side effects, we may refactor it out (but that is outside the scope of this pass)
                        return ParentUseCase(ParentNonSpecial, e);
                    }
                    e = parent;
                } else if (allparents[0].get<VarDecl>()!=NULL) {
                    const Expr *vd = allparents[0].get<VarDecl>()->getInit();
                    if (vd==e) {
                        //not an asignment proper, but we can treat it like one...
                        return ParentUseCase(ParentAssignmentRHS, vd);
                    } else {
                        //parent is a VarDecl, but something funky is going on...
                        return ParentUseCase(ParentNonSpecial, e);
                    }
                } else {
                    //parent is not a Stmt
                    return ParentUseCase(ParentNonSpecial, e);
                }
            } else {
                //number of parents != 1
                return ParentUseCase(ParentNonSpecial, e);
            }
        }
        //parent is a CompoundStmt
        return ParentUseCase(ParentNonSpecial, e);
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

    CondResult simplePartialEvaluation(const Expr *sub, bool value, const Expr *whole) {
        const Stmt *s = sub, *toReturn = sub, *parent;
        bool keepSearching = true;
        while(s != whole && keepSearching) {
            const auto& allparents = context->getParents(*s);
            if (allparents.size()!=1) {
                llvm::errs() << "expression ancestry different than expected! At one point it has " << allparents.size() << " simultaneous ancestors!!!\n Bailing out of unwinding process...";
                showParents(s);
                keepSearching = false;
            } else {
                parent = allparents[0].get<Stmt>();
                if (isa<ParenExpr>(parent)        ||
                    isa<ImplicitCastExpr>(parent)         ||
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
                } //TODO: add support for more operators (and detect if we are actually nested in another condition (i.e. a ConditionalOperator nested in the condition of another one or the one of an IfStmt)
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
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(RefactorEngine *R) : handler(R, TermName.getValue()) {
        // Add a simple matcher for finding calls to config functions.
        Matcher.addMatcher(
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
                     ).bind("callToConfigFunction"),
            &handler
        );

    }

  void HandleTranslationUnit(ASTContext &Context) override {
    handler.setContext(&Context);
    // Run the matcher when we have the whole TU parsed.
    Matcher.matchAST(Context);
    //TODO: here we might do further processing if required, such as creating and using new matchers/handlers or visitors for:
    //   * refactoring out boolean variables which are given values based on config functions whose assignments have no side effects
    //   * refactoring out simple boolean functions
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
