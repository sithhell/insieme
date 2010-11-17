/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#include "insieme/frontend/conversion.h"

#include "insieme/frontend/utils/source_locations.h"
#include "insieme/frontend/utils/dep_graph.h"
#include "insieme/frontend/analysis/expr_analysis.h"
#include "insieme/frontend/omp/omp_pragma.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/logging.h"

#include "insieme/core/lang_basic.h"
#include "insieme/core/transform/node_replacer.h"

#include "insieme/c_info/naming.h"

#include "clang/AST/StmtVisitor.h"

#include "clang/Index/Entity.h"
#include "clang/Index/Indexer.h"

using namespace clang;
using namespace insieme;
namespace fe = insieme::frontend;

namespace {
// Returns a string of the text within the source range of the input stream
std::string GetStringFromStream(const SourceManager& srcMgr, const SourceLocation& start) {
	// we use the getDecomposedSpellingLoc() method because in case we read macros values
	// we have to read the expanded value
	std::pair<FileID, unsigned> startLocInfo = srcMgr.getDecomposedSpellingLoc(start);
	llvm::StringRef startBuffer = srcMgr.getBufferData(startLocInfo.first);
	const char *strDataStart = startBuffer.begin() + startLocInfo.second;

	return string(strDataStart, clang::Lexer::MeasureTokenLength(srcMgr.getSpellingLoc(start), srcMgr, clang::LangOptions()));
}

// in case the the last argument of the function is a var_arg, we try pack the exceeding arguments with the pack
// operation provided by the IR.
vector<core::ExpressionPtr> tryPack(const core::ASTBuilder& builder, core::FunctionTypePtr funcTy, const ExpressionList& args) {

	// check if the function type ends with a VAR_LIST type
	core::TupleTypePtr&& argTy = core::dynamic_pointer_cast<const core::TupleType>(funcTy->getArgumentType());
	assert(argTy && "Function argument is of not type TupleType");

	const core::TupleType::ElementTypeList& elements = argTy->getElementTypes();
	// if the tuple type is empty it means we cannot pack any of the arguments
	if( elements.empty() )
		return args;

	if(*elements.back() == core::lang::TYPE_VAR_LIST_VAL) {
		ExpressionList ret;
		assert(args.size() >= elements.size()-1 && "Function called with fewer arguments than necessary");
		// last type is a var_list, we have to do the packing of arguments

		// we copy the first N-1 arguments, the remaining will be unpacked
		std::copy(args.begin(), args.begin()+elements.size()-1, std::back_inserter(ret));

		ExpressionList toPack;
		if(args.size() > elements.size()-1) {
			std::copy(args.begin()+elements.size()-1, args.end(), std::back_inserter(toPack));
		}

		// arguments has to be packed into a tuple expression, and then inserted into a pack expression
		ret.push_back(
			builder.callExpr(core::lang::TYPE_VAR_LIST_PTR, core::lang::OP_VAR_LIST_PACK_PTR, toVector<core::ExpressionPtr>(builder.tupleExpr(toPack)))
		);
		return ret;
	}
	return args;
}

// FIXME: this has to be rewritten once lang/core is in a final state
std::string getOperationType(const core::TypePtr& type) {
	using namespace core::lang;
	DVLOG(2) << type;
	if(isUIntType(*type))	return "uint";
	if(isIntType(*type)) 	return "int";
	if(isBoolType(*type))	return "bool";
	if(isRealType(*type))	return "real";
    if(isVectorType(*type)) {
        const core::VectorType* vt = dynamic_cast<const core::VectorType*>(&*type);

        const core::TypePtr ref = vt->getElementType();
        std::ostringstream ss;

        if(const core::RefType* subtype = dynamic_cast<const core::RefType*>(&*ref))
            ss << "vector<" << getOperationType(subtype->getElementType()) << ">";
        else
            ss << "vector<" << getOperationType(ref) << ">";

//        ss << "vector<" << getOperationType(vt->getElementType()) << ">";
        return ss.str();
    }
    // FIXME
    return "unit";
	// assert(false && "Type not supported");
}

}

namespace insieme {
namespace frontend {
namespace conversion {

#define START_LOG_EXPR_CONVERSION(expr) \
	assert(convFact.currTU && "Translation unit not correctly set"); \
	DVLOG(1) << "\n****************************************************************************************\n" \
			 << "Converting expression [class: '" << expr->getStmtClassName() << "']\n" \
			 << "-> at location: (" << utils::location(expr->getLocStart(), convFact.currTU->getCompiler().getSourceManager()) << "): "; \
	if( VLOG_IS_ON(2) ) { \
		DVLOG(2) << "Dump of clang expression: \n" \
				 << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"; \
		expr->dump(); \
	}

#define END_LOG_EXPR_CONVERSION(expr) \
	DVLOG(1) << "Converted into IR expression: "; \
	DVLOG(1) << "\t" << *expr;


// creates a function call from a list of expressions,
// usefull for implementing the semantics of ++ or -- or comma separated expressions in the IR
core::CallExprPtr ConversionFactory::createCallExpr(const StatementList& body, core::TypePtr retTy) const {

	core::CompoundStmtPtr&& bodyStmt = builder.compoundStmt( body );
	// keeps the list variables used in the body
	insieme::frontend::analysis::VarRefFinder args(bodyStmt);

	core::Lambda::CaptureList capture;
	core::CaptureInitExpr::Initializations initializations;
	std::for_each(args.begin(), args.end(),
		[ &capture, &builder, &bodyStmt, &initializations ] (const core::ExpressionPtr& curr) {
			const core::VariablePtr& bodyVar = core::dynamic_pointer_cast<const core::Variable>(curr);
			core::VariablePtr&& parmVar = builder.variable( bodyVar->getType() );
			capture.push_back( parmVar );
			initializations.insert( std::make_pair(parmVar, bodyVar) );
			// we have to replace the variable of the body with the newly created parmVar
			bodyStmt = core::dynamic_pointer_cast<const core::CompoundStmt>(
					core::transform::replaceAll(builder.getNodeManager(), bodyStmt, bodyVar, parmVar, true)
			);
			assert(bodyStmt);
		}
	);

	// build the type of the function
	core::FunctionTypePtr&& funcTy = builder.functionType( builder.tupleType( core::TupleType::ElementTypeList() ), retTy);

	// build the expression body
	core::LambdaExprPtr&& lambdaExpr = builder.lambdaExpr( funcTy, capture, core::Lambda::ParamList(), bodyStmt );
	core::CaptureInitExprPtr&& retExpr = builder.captureInitExpr(lambdaExpr, initializations);

	return builder.callExpr( retTy, retExpr, ExpressionList() );
}

//#############################################################################
//
//							CLANG EXPRESSION CONVERTER
//
//############################################################################
class ConversionFactory::ClangExprConverter: public StmtVisitor<ClangExprConverter, core::ExpressionPtr> {
	ConversionFactory& convFact;
	ConversionContext& ctx;
public:

	// CallGraph for functions, used to resolved eventual recursive functions
	utils::DependencyGraph<const clang::FunctionDecl*> funcDepGraph;

	ClangExprConverter(ConversionFactory& convFact): convFact(convFact), ctx(convFact.ctx) { }

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								INTEGER LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitIntegerLiteral(clang::IntegerLiteral* intLit) {
		START_LOG_EXPR_CONVERSION(intLit);
		core::ExpressionPtr&& retExpr =
			convFact.builder.literal(
				// retrieve the string representation from the source code
				GetStringFromStream( convFact.currTU->getCompiler().getSourceManager(), intLit->getExprLoc()),
				convFact.convertType( GET_TYPE_PTR(intLit) )
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								FLOATING LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitFloatingLiteral(clang::FloatingLiteral* floatLit) {
		START_LOG_EXPR_CONVERSION(floatLit);
		core::ExpressionPtr&& retExpr =
			// retrieve the string representation from the source code
			convFact.builder.literal(
				GetStringFromStream( convFact.currTU->getCompiler().getSourceManager(), floatLit->getExprLoc()),
				convFact.convertType( GET_TYPE_PTR(floatLit) )
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								CHARACTER LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCharacterLiteral(CharacterLiteral* charLit) {
		START_LOG_EXPR_CONVERSION(charLit);
		core::ExpressionPtr&& retExpr =
			convFact.builder.literal(
				// retrieve the string representation from the source code
				GetStringFromStream(convFact.currTU->getCompiler().getSourceManager(), charLit->getExprLoc()),
					(charLit->isWide() ? convFact.builder.genericType("wchar") : convFact.builder.genericType("char"))
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								STRING LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitStringLiteral(clang::StringLiteral* stringLit) {
		START_LOG_EXPR_CONVERSION(stringLit);
		core::ExpressionPtr&& retExpr =
			convFact.builder.literal(
				GetStringFromStream( convFact.currTU->getCompiler().getSourceManager(), stringLit->getExprLoc()),
				convFact.builder.genericType(core::Identifier("string"))
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								CXX BOOLEAN LITERAL
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCXXBoolLiteralExpr(CXXBoolLiteralExpr* boolLit) {
		START_LOG_EXPR_CONVERSION(boolLit);
		core::ExpressionPtr&& retExpr =
			// retrieve the string representation from the source code
			convFact.builder.literal(
				GetStringFromStream(convFact.currTU->getCompiler().getSourceManager(), boolLit->getExprLoc()), core::lang::TYPE_BOOL_PTR
			);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							PARENTESIS EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitParenExpr(clang::ParenExpr* parExpr) {
		return Visit( parExpr->getSubExpr() );
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						   IMPLICIT CAST EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitImplicitCastExpr(clang::ImplicitCastExpr* implCastExpr) {
		START_LOG_EXPR_CONVERSION(implCastExpr);
		const core::TypePtr& type = convFact.convertType( GET_TYPE_PTR(implCastExpr) );
		core::ExpressionPtr&& subExpr = Visit(implCastExpr->getSubExpr());
		core::ExpressionPtr&& nonRefExpr = convFact.tryDeref(subExpr);

		// if the subexpression is an array or a vector, remove all the C implicit casts
		if( dynamic_pointer_cast<const core::ArrayType>(nonRefExpr->getType()) ||
			dynamic_pointer_cast<const core::VectorType>(nonRefExpr->getType()) )
			return subExpr;

		// In the case the target type of the cast is not a reftype we deref the subexpression
		if(!core::dynamic_pointer_cast<const core::RefType>(type)) {
			subExpr = convFact.tryDeref(subExpr);
		}
		core::ExpressionPtr&& retExpr = convFact.builder.castExpr( type, subExpr );
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//								CAST EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCastExpr(clang::CastExpr* castExpr) {
		START_LOG_EXPR_CONVERSION(castExpr);
		const core::TypePtr& type = convFact.convertType( GET_TYPE_PTR(castExpr) );
		core::ExpressionPtr&& subExpr = Visit(castExpr->getSubExpr());
		// In the case the target type of the cast is not a reftype we deref the subexpression
		if(!core::dynamic_pointer_cast<const core::RefType>(type)) {
			subExpr = convFact.tryDeref(subExpr);
		}
		core::ExpressionPtr&& retExpr = convFact.builder.castExpr( type, subExpr );
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							FUNCTION CALL EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCallExpr(clang::CallExpr* callExpr) {
		START_LOG_EXPR_CONVERSION(callExpr);
		if( FunctionDecl* funcDecl = dyn_cast<FunctionDecl>(callExpr->getDirectCallee()) ) {
			const core::ASTBuilder& builder = convFact.builder;

			core::FunctionTypePtr&& funcTy = core::dynamic_pointer_cast<const core::FunctionType>( convFact.convertType( GET_TYPE_PTR(funcDecl) ) );
			// collects the type of each argument of the expression
			ExpressionList args;
//			const core::TupleType::ElementTypeList& argTypes = funcTy->getArgumentType()->getElementTypes();

			for(size_t argId = 0, end = callExpr->getNumArgs(); argId < end; ++argId) {
				Expr* currArg = callExpr->getArg(argId);
				core::ExpressionPtr&& arg = this->Visit(currArg);
				arg = this->convFact.tryDeref(arg);
				args.push_back( arg );
			}

			ExpressionList&& packedArgs = tryPack(convFact.builder, funcTy, args);

			const TranslationUnit* oldTU = convFact.currTU;

			const FunctionDecl* definition = NULL;
			// this will find function definitions if they are declared in  the same translation unit (also defined as static)
			if( !funcDecl->hasBody(definition) ) {
				// if the function is not defined in this translation unit, maybe it is defined in another we already loaded
				// use the clang indexer to lookup the definition for this function declarations
				clang::idx::Entity&& funcEntity = clang::idx::Entity::get(funcDecl, const_cast<clang::idx::Program&>(convFact.program.getClangProgram()));
				std::pair<FunctionDecl*, clang::idx::TranslationUnit*>&& ret = convFact.program.getClangIndexer().getDefinitionFor(funcEntity);
				if(ret.first) {
					definition = ret.first;
					assert(ret.second);
					convFact.currTU = &Program::getTranslationUnit(ret.second);
				}
			}
			if(!definition) {
				// No definition has been found in any of the translation units, we mark this function as extern!
				core::ExpressionPtr irNode =
						convFact.builder.callExpr(	funcTy->getReturnType(), builder.literal(funcDecl->getNameAsString(), funcTy), packedArgs );
				// handle eventual pragmas attached to the Clang node
				frontend::omp::attachOmpAnnotation(irNode, callExpr, convFact);
				return irNode;
			}

			// We find a definition, we lookup if this variable needs to access the globals, in that case the
			// capture list needs to be initialized with the value of global variable in the current scope
			core::CaptureInitExpr::Initializations initializations;
			if(ctx.globalFuncMap.find(definition) != ctx.globalFuncMap.end()) {
				// we expect to have a the currGlobalVar set to the value of the var keeping global definitions
				// in the current context
				assert(ctx.currGlobalVar && "No global definitions forwarded to this point");
				auto fit = ctx.funcGlobalCaptureMap.find(definition);
				assert(fit != ctx.funcGlobalCaptureMap.end());
				initializations.insert( std::make_pair(fit->second, ctx.currGlobalVar) );
			}

			// If we are resolving the body of a recursive function we have to return the associated
			// variable every time a function in the strongly connected graph of function calls
			// is encountred.
			if(ctx.isResolvingRecFuncBody) {
				// check if this type has a typevar already associated, in such case return it
				ConversionContext::RecVarExprMap::const_iterator fit = ctx.recVarExprMap.find(definition);
				if( fit != ctx.recVarExprMap.end() ) {
					// we are resolving a parent recursive type, so when one of the recursive functions in the
					// connected components are called, the introduced mu variable has to be used instead.
					convFact.currTU = oldTU;
					core::ExpressionPtr callee;
					if(!initializations.empty())
						callee = builder.captureInitExpr(fit->second, initializations);
					else
						callee = fit->second;
					return builder.callExpr(funcTy->getReturnType(), callee, packedArgs);
				}
			}

			if(!ctx.isResolvingRecFuncBody) {
				ConversionContext::LambdaExprMap::const_iterator fit = ctx.lambdaExprCache.find(definition);
				if(fit != ctx.lambdaExprCache.end()) {
					convFact.currTU = oldTU;
					core::ExpressionPtr callee;
					if(!initializations.empty())
						callee = builder.captureInitExpr(fit->second, initializations);
					else
						callee = fit->second;
					core::ExpressionPtr&& irNode = builder.callExpr(funcTy->getReturnType(), callee, packedArgs);
					// handle eventual pragmas attached to the Clang node
					frontend::omp::attachOmpAnnotation(irNode, callExpr, convFact);
					return irNode;
				}
			}

			assert(definition && "No definition found for function");
			core::ExpressionPtr&& lambdaExpr = core::dynamic_pointer_cast<const core::LambdaExpr>(convFact.convertFunctionDecl(definition));
			assert(lambdaExpr && "Call expression resulted in a empty lambda expression");
			convFact.currTU = oldTU;

			// initialize the capture list
			if(!initializations.empty())
				lambdaExpr = builder.captureInitExpr(lambdaExpr, initializations);

			core::ExpressionPtr irNode = builder.callExpr(funcTy->getReturnType(), lambdaExpr, packedArgs);
			// handle eventual pragmas attached to the Clang node
			frontend::omp::attachOmpAnnotation(irNode, callExpr, convFact);
			return irNode;
		}
		assert(false && "Call expression not referring a function");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						CXX MEMBER CALL EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCXXMemberCallExpr(clang::CXXMemberCallExpr* callExpr) {
		//todo: CXX extensions
		assert(false && "CXXMemberCallExpr not yet handled");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						CXX OPERATOR CALL EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitCXXOperatorCallExprr(clang::CXXOperatorCallExpr* callExpr) {
		//todo: CXX extensions
		assert(false && "CXXOperatorCallExpr not yet handled");
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							BINARY OPERATOR
	//
	// [C99 6.5.2.3] Structure and Union Members. X->F and X.F.
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitMemberExpr(clang::MemberExpr* membExpr)  {
		START_LOG_EXPR_CONVERSION(membExpr);
		const core::ASTBuilder& builder = convFact.builder;

		core::ExpressionPtr&& base = convFact.tryDeref(Visit(membExpr->getBase()));
		if(membExpr->isArrow()) {
			// we have to check whether we currently have a ref or probably an array (which is used to represent C pointers)
			base = convFact.tryDeref(Visit(membExpr->getBase()));
			DLOG(INFO) << *base->getType();
			if(core::dynamic_pointer_cast<const core::VectorType>(base->getType()) ||
				core::dynamic_pointer_cast<const core::ArrayType>(base->getType())) {

				core::SingleElementTypePtr&& subTy = core::dynamic_pointer_cast<const core::SingleElementType>(base->getType());
				assert(subTy);
				base = builder.callExpr( subTy->getElementType(), core::lang::OP_SUBSCRIPT_SINGLE_PTR,
						toVector<core::ExpressionPtr>(base, builder.literal("0", core::lang::TYPE_INT_4_PTR)) );
				base = convFact.tryDeref(base);
			}
			DLOG(INFO) << *base->getType();
		}
		core::Identifier&& ident = membExpr->getMemberDecl()->getNameAsString();

		core::ExpressionPtr&& retExpr = builder.memberAccessExpr(base, ident);
		// handle eventual pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(retExpr, membExpr, convFact);

		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							BINARY OPERATOR
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitBinaryOperator(clang::BinaryOperator* binOp)  {
		START_LOG_EXPR_CONVERSION(binOp);
		const core::ASTBuilder& builder = convFact.builder;

 		core::ExpressionPtr&& rhs = Visit(binOp->getRHS());
		core::ExpressionPtr&& lhs = Visit(binOp->getLHS());

		// if the binary operator is a comma separated expression, we convert it into a function call
		// which returns the value of the last expression
		if( binOp->getOpcode() == BO_Comma) {
			return convFact.createCallExpr(toVector<core::StatementPtr>(lhs, builder.returnStmt(rhs)), rhs->getType());
		}

		core::TypePtr&& exprTy = convFact.convertType( GET_TYPE_PTR(binOp) );

		// create Pair type
		core::TupleTypePtr&& tupleTy = builder.tupleType(toVector( exprTy, exprTy ) );
		std::string&& opType = getOperationType(exprTy);

		// we take care of compound operators first,
		// we rewrite the RHS expression in a normal form, i.e.:
		// a op= b  ---->  a = a op b
		std::string op;
		clang::BinaryOperatorKind baseOp;
		switch( binOp->getOpcode() ) {
		// a *= b
		case BO_MulAssign: 	op = "mul"; baseOp = BO_Mul; break;
		// a /= b
		case BO_DivAssign: 	op = "div"; baseOp = BO_Div; break;
		// a %= b
		case BO_RemAssign:	op = "mod"; baseOp = BO_Rem; break;
		// a += b
		case BO_AddAssign: 	op = "add"; baseOp = BO_Add; break;
		// a -= b
		case BO_SubAssign:	op = "sub"; baseOp = BO_Sub; break;
		// a <<= b
		case BO_ShlAssign: 	op = "shl"; baseOp = BO_Shl; break;
		// a >>= b
		case BO_ShrAssign: 	op = "shr"; baseOp = BO_Shr; break;
		// a &= b
		case BO_AndAssign: 	op = "and"; baseOp = BO_And; break;
		// a |= b
		case BO_OrAssign: 	op = "or"; 	baseOp = BO_Or; break;
		// a ^= b
		case BO_XorAssign: 	op = "xor"; baseOp = BO_Xor; break;
		default:
			break;
		}

		if( !op.empty() ) {
			// The operator is a compound operator, we substitute the RHS expression with the expanded one
			// core::RefTypePtr&& lhsTy = core::dynamic_pointer_cast<const core::RefType>(lhs->getType());
			// assert( lhsTy && "LHS operand must of type ref<a'>." );
			core::lang::OperatorPtr&& opFunc = builder.literal( opType + "." + op, builder.functionType(tupleTy, exprTy));

			// we check if the RHS is a ref, in that case we use the deref operator
			rhs = convFact.tryDeref(rhs);
			core::ExpressionPtr&& subExprLHS = convFact.tryDeref(lhs);

			const core::TypePtr& lhsSubTy = subExprLHS->getType();
			rhs = builder.callExpr(lhsSubTy, opFunc,
				toVector<core::ExpressionPtr>(builder.callExpr( lhsSubTy, core::lang::OP_REF_DEREF_PTR, toVector(lhs) ), rhs) );

			// add an annotation to the subexpression
			opFunc->addAnnotation( std::make_shared<c_info::COpAnnotation>( BinaryOperator::getOpcodeStr(baseOp)) );
		}

		bool isAssignment = false;
		bool isLogical = false;
		baseOp = binOp->getOpcode();

		core::lang::OperatorPtr opFunc;

		switch( binOp->getOpcode() ) {
		case BO_PtrMemD:
		case BO_PtrMemI:
			assert(false && "Operator not yet supported!");

		// a * b
		case BO_Mul: 	op = "mul";  break;
		// a / b
		case BO_Div: 	op = "div";  break;
		// a % b
		case BO_Rem: 	op = "mod";  break;
		// a + b
		case BO_Add: 	op = "add";  break;
		// a - b
		case BO_Sub: 	op = "sub";  break;
		// a << b
		case BO_Shl: 	op = "shl";  break;
		// a >> b
		case BO_Shr: 	op = "shr";  break;
		// a & b
		case BO_And: 	op = "and";  break;
		// a ^ b
		case BO_Xor: 	op = "xor";  break;
		// a | b
		case BO_Or:  	op = "or"; 	 break;

		// Logic operators

		// a && b
		case BO_LAnd: 	op = "land"; isLogical=true; break;
		// a || b
		case BO_LOr:  	op = "lor";  isLogical=true; break;
		// a < b
		case BO_LT:	 	op = "lt";   isLogical=true; break;
		// a > b
		case BO_GT:  	op = "gt";   isLogical=true; break;
		// a <= b
		case BO_LE:  	op = "le";   isLogical=true; break;
		// a >= b
		case BO_GE:  	op = "ge";   isLogical=true; break;
		// a == b
		case BO_EQ:  	op = "eq";   isLogical=true; break;
		// a != b
		case BO_NE:	 	op = "ne";   isLogical=true; break;

		case BO_MulAssign: case BO_DivAssign: case BO_RemAssign: case BO_AddAssign: case BO_SubAssign:
		case BO_ShlAssign: case BO_ShrAssign: case BO_AndAssign: case BO_XorAssign: case BO_OrAssign:
		case BO_Assign:
		{
			baseOp = BO_Assign;
			// poor C codes assign value to function parameters, this is not allowed here as input parameters
			// are of non REF type. What we need to do is introduce a declaration for these variables
			// and use the created variable on the stack instead of the input prameters
			DeclRefExpr* ref = dyn_cast<DeclRefExpr>(binOp->getLHS());
			if(ref && isa<ParmVarDecl>(ref->getDecl())) {
				core::VariablePtr&& parmVar = core::dynamic_pointer_cast<const core::Variable>(lhs);
				assert(parmVar);
				// look up the needRef map to see if we already introduced a declaration for this variable
				auto fit = ctx.needRef.find(parmVar);
				if(fit == ctx.needRef.end()) {
					fit = ctx.needRef.insert( std::make_pair(parmVar, builder.variable(builder.refType(parmVar->getType()))) ).first;
				}
				lhs = fit->second;
			}

			// This is an assignment, we have to make sure the LHS operation is of type ref<a'>
			assert( core::dynamic_pointer_cast<const core::RefType>(lhs->getType()) && "LHS operand must of type ref<a'>." );
			isAssignment = true;
			opFunc = convFact.mgr.get(core::lang::OP_REF_ASSIGN_PTR);
			exprTy = core::lang::TYPE_UNIT_PTR;
			break;
		}
		default:
			assert(false && "Operator not supported");
		}

		// build a callExpr with the 2 arguments
		rhs = convFact.tryDeref(rhs);

		if( !isAssignment )
			lhs = convFact.tryDeref(lhs);

		// check the types
//		const core::TupleTypePtr& opTy = core::dynamic_pointer_cast<const core::FunctionType>(opFunc->getType())->getArgumentType();
//		if(lhs->getType() != opTy->getElementTypes()[0]) {
//			// add a castepxr
//			lhs = convFact.builder.castExpr(opTy->getElementTypes()[0], lhs);
//		}
//		if(rhs->getType() != opTy->getElementTypes()[1]) {
//			// add a castepxr
//			rhs = convFact.builder.castExpr(opTy->getElementTypes()[1], rhs);
//		}

		if(isLogical) {
			exprTy = core::lang::TYPE_BOOL_PTR;
			tupleTy = builder.tupleType(toVector(lhs->getType(), rhs->getType())); // FIXME
		}

		if(!isAssignment)
			opFunc = builder.literal( opType + "." + op, builder.functionType(tupleTy, exprTy));

		core::ExpressionPtr&& retExpr = convFact.builder.callExpr( exprTy, opFunc, toVector(lhs, rhs) );

		// add the operator name in order to help the convertion process in the backend
		opFunc->addAnnotation( std::make_shared<c_info::COpAnnotation>( BinaryOperator::getOpcodeStr(baseOp) ) );

		// handle eventual pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(retExpr, binOp, convFact);

		END_LOG_EXPR_CONVERSION( retExpr );
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							UNARY OPERATOR
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitUnaryOperator(clang::UnaryOperator *unOp) {
		START_LOG_EXPR_CONVERSION(unOp);
		const core::ASTBuilder& builder = convFact.builder;
		core::ExpressionPtr&& subExpr = Visit(unOp->getSubExpr());

		// build lambda expression for post/pre increment/decrement unary operators
		auto encloseIncrementOperator = [ this, &builder ](core::ExpressionPtr subExpr, bool post, bool additive) {
			core::RefTypePtr expTy = core::dynamic_pointer_cast<const core::RefType>(subExpr->getType());
			assert( expTy && "LHS operand must of type ref<a'>." );
			const core::TypePtr& subTy = expTy->getElementType();

			core::VariablePtr tmpVar;
			std::vector<core::StatementPtr> stmts;
			if(post) {
				tmpVar = builder.variable(subTy);
				// ref<a'> __tmp = subexpr
				stmts.push_back(builder.declarationStmt(tmpVar,
						builder.callExpr( subTy, core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(subExpr) ) ));
			}
			// subexpr op= 1
			stmts.push_back(
				builder.callExpr(
					core::lang::TYPE_UNIT_PTR,
					core::lang::OP_REF_ASSIGN_PTR,
					toVector<core::ExpressionPtr>(
						subExpr, // ref<a'> a
						builder.callExpr(
							subTy,
							( additive ? core::lang::OP_INT_ADD_PTR : core::lang::OP_INT_SUB_PTR ),
								toVector<core::ExpressionPtr>(
									builder.callExpr( subTy, core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(subExpr) ),
									builder.castExpr( subTy, builder.literal("1", core::lang::TYPE_INT_4_PTR))
								)
							) // a - 1
					)
				)
			);
			if(post) {
				assert(tmpVar);
				// return __tmp
				stmts.push_back( builder.returnStmt( tmpVar ) );
			} else {
				// return the variable
				stmts.push_back( builder.callExpr( subTy, core::lang::OP_REF_DEREF_PTR, toVector<core::ExpressionPtr>(subExpr) ) );
			}
			return this->convFact.createCallExpr(std::vector<core::StatementPtr>(stmts), subTy);
		};

		bool post = true;
		switch(unOp->getOpcode()) {
		// conversion of post increment/decrement operation is done by creating a tuple expression i.e.:
		// a++ ==> (__tmp = a, a=a+1, __tmp)
		// ++a ==> ( a=a+1, a)
		// --a
		case UO_PreDec:
			post = false;
		// a--
		case UO_PostDec:
			subExpr = encloseIncrementOperator(subExpr, post, false);
			break;
		// a++
		case UO_PreInc:
			post = false;
		// ++a
		case UO_PostInc:
			subExpr = encloseIncrementOperator(subExpr, post, true);
			break;
		// &a
		case UO_AddrOf:
		{
			// We need to be carefull paramvars are not dereferenced and the address passed around. If this happens
			// we have to declare a variable holding the memory location for that value and replace every use of
			// the paramvar with the newly generated variable: the structure needRef in the ctx is used for this
			DeclRefExpr* ref = dyn_cast<DeclRefExpr>(unOp->getSubExpr());
			if(ref && isa<ParmVarDecl>(ref->getDecl())) {
				core::VariablePtr&& parmVar = core::dynamic_pointer_cast<const core::Variable>(subExpr);
				assert(parmVar);
				// look up the needRef map to see if we already introduced a declaration for this variable
				auto fit = ctx.needRef.find(parmVar);
				if(fit == ctx.needRef.end()) {
					fit = ctx.needRef.insert( std::make_pair(parmVar, builder.variable(builder.refType(parmVar->getType()))) ).first;
				}
				// we make it a vector FIXME
				subExpr = fit->second;
			}
			subExpr = builder.vectorExpr( toVector<core::ExpressionPtr>(subExpr) );
			break;
		}
		// *a
		case UO_Deref: {
			subExpr = convFact.tryDeref(subExpr);

			if(core::dynamic_pointer_cast<const core::VectorType>(subExpr->getType()) ||
				core::dynamic_pointer_cast<const core::ArrayType>(subExpr->getType())) {

				core::SingleElementTypePtr&& subTy = core::dynamic_pointer_cast<const core::SingleElementType>(subExpr->getType());
				assert(subTy);
				subExpr = builder.callExpr( subTy->getElementType(), core::lang::OP_SUBSCRIPT_SINGLE_PTR,
						toVector<core::ExpressionPtr>(subExpr, builder.literal("0", core::lang::TYPE_INT_4_PTR)) );
			}
			break;
		}
		// +a
		case UO_Plus:
			// just return the subexpression
			break;
		// -a
		case UO_Minus:
			// TODO:
			// assert(false && "Conversion of unary operator '-' not supported");
		// ~a
		case UO_Not:
			// TODO:
			// assert(false && "Conversion of unary operator '~' not supported");
		// !a
		case UO_LNot:
			// TODO:
			// assert(false && "Conversion of unary operator '!' not supported");

		case UO_Real:
		case UO_Imag:
		case UO_Extension: //TODO:
		default:
			break;
			// assert(false && "Unary operator not supported");
		}

		// handle eventual pragmas attached to the Clang node
		frontend::omp::attachOmpAnnotation(subExpr, unOp, convFact);

		// add the operator name in order to help the convertion process in the backend
		subExpr->addAnnotation( std::make_shared<c_info::COpAnnotation>( UnaryOperator::getOpcodeStr(unOp->getOpcode()) ) );

		return subExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							CONDITIONAL OPERATOR
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitConditionalOperator(clang::ConditionalOperator* condOp) {
		START_LOG_EXPR_CONVERSION(condOp);
		const core::ASTBuilder& builder = convFact.builder;
		assert(!condOp->getSaveExpr() && "Conditional operation with 'gcc save' expession not supperted.");
		core::TypePtr&& retTy = convFact.convertType( GET_TYPE_PTR(condOp) );

		core::ExpressionPtr&& trueExpr = Visit(condOp->getTrueExpr());
		core::ExpressionPtr&& falseExpr = Visit(condOp->getFalseExpr());
		core::ExpressionPtr&& condExpr = Visit( condOp->getCond() );

		// add ref.deref if needed
		condExpr = convFact.tryDeref(condExpr);

		if(*condExpr->getType() != core::lang::TYPE_BOOL_VAL) {
			// the return type of the condition is not a boolean, we add a cast expression
			condExpr = builder.castExpr(core::lang::TYPE_BOOL_PTR, condExpr);
		}

		// builder.callExpr(retTy, core::lang::OP_ITE_PTR, )
		core::StatementPtr&& ifStmt = builder.ifStmt(condExpr, builder.returnStmt(trueExpr), builder.returnStmt(falseExpr));
		core::ExpressionPtr&& retExpr = convFact.createCallExpr( toVector( ifStmt ),  retTy);
		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						ARRAY SUBSCRIPT EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitArraySubscriptExpr(clang::ArraySubscriptExpr* arraySubExpr) {
		START_LOG_EXPR_CONVERSION(arraySubExpr);

		// CLANG introduces implicit cast for the base expression of array subscripts
		// which cast the array type into a simple pointer. As insieme supports subscripts
		// only for array or vector types, we skip eventual implicit cast operations
		Expr* baseExpr = arraySubExpr->getBase();
		while(ImplicitCastExpr *castExpr = dyn_cast<ImplicitCastExpr>(baseExpr))
			baseExpr = castExpr->getSubExpr();

		// IDX
		core::ExpressionPtr&& idx = convFact.tryDeref(Visit( arraySubExpr->getIdx() ));

		// BASE
		core::ExpressionPtr&& base = convFact.tryDeref(Visit( baseExpr ) );

		// TODO: we need better checking for vector type
		assert( (core::dynamic_pointer_cast<const core::VectorType>( base->getType() ) ||
				core::dynamic_pointer_cast<const core::ArrayType>( base->getType() )) &&
				"Base expression of array subscript is not a vector/array type.");

		// We are sure the type of base is either a vector or an array
		const core::TypePtr& subTy = core::dynamic_pointer_cast<const core::SingleElementType>(base->getType())->getElementType();

		core::ExpressionPtr&& retExpr =
			convFact.builder.callExpr( subTy, core::lang::OP_SUBSCRIPT_SINGLE_PTR,
					toVector<core::ExpressionPtr>(base, convFact.builder.castExpr(core::lang::TYPE_UINT_4_PTR, idx)) );

		END_LOG_EXPR_CONVERSION(retExpr);
		return retExpr;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						EXT VECTOR ELEMENT EXPRESSION
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitExtVectorElementExpr(ExtVectorElementExpr* vecElemExpr){
        START_LOG_EXPR_CONVERSION(vecElemExpr);
        core::ExpressionPtr&& base = Visit( vecElemExpr->getBase() );

        std::string pos;
        llvm::StringRef&& accessor = vecElemExpr->getAccessor().getName();

        //translate OpenCL accessor string to index
        if(accessor == "x") 		pos = "0";
        else if(accessor == "y")    pos = "1";
        else if(accessor == "z")	pos = "2";
        else if(accessor == "w")	pos = "3";
        else {
        	// the input string is in a form sXXX
        	assert(accessor.front() == 's');
        	// we skip the s and return the value to get the number
        	llvm::StringRef numStr = accessor.substr(1,accessor.size()-1);
        	assert(insieme::utils::numeric_cast<unsigned int>(numStr.data()) >= 0 && "String is not a number");
        	pos = numStr;
        }

        core::TypePtr&& exprTy = convFact.convertType( GET_TYPE_PTR(vecElemExpr) );
        core::ExpressionPtr&& idx = convFact.builder.literal(pos, exprTy); // FIXME! are you sure the type is exprTy? and not ref<rexprTy>?
        // if the type of the vector is a refType, we deref it
        base = convFact.tryDeref(base);

        core::ExpressionPtr&& retExpr = convFact.builder.callExpr(convFact.builder.refType(exprTy), core::lang::OP_SUBSCRIPT_SINGLE_PTR, toVector( base, idx ));
        END_LOG_EXPR_CONVERSION(retExpr);
        return retExpr;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							VAR DECLARATION REFERENCE
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitDeclRefExpr(clang::DeclRefExpr* declRef) {
		START_LOG_EXPR_CONVERSION(declRef);
		// check whether this is a reference to a variable
		if(VarDecl* varDecl = dyn_cast<VarDecl>(declRef->getDecl())) {
			return convFact.lookUpVariable(varDecl);
		}
		// todo: C++ check whether this is a reference to a class field, or method (function).
		assert(false && "DeclRefExpr not supported!");
	}

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //                  VECTOR/STRUCT INITALIZATION EXPRESSION
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	core::ExpressionPtr VisitInitListExpr(clang::InitListExpr* initList) {
		assert(false && "Visiting of initializer list is not allowed!");
    }
};

ConversionFactory::ClangExprConverter* ConversionFactory::makeExprConverter(ConversionFactory& fact) {
	return new ClangExprConverter(fact);
}

core::ExpressionPtr ConversionFactory::convertExpr(const clang::Expr* expr) const {
	assert(expr && "Calling convertExpr with a NULL pointer");
	return exprConv->Visit( const_cast<Expr*>(expr) );
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//						CONVERT FUNCTION DECLARATION
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
core::NodePtr ConversionFactory::convertFunctionDecl(const clang::FunctionDecl* funcDecl, bool isEntryPoint) {
	// the function is not extern, a lambdaExpr has to be created
	assert(funcDecl->hasBody() && "Function has no body!");
	assert(currTU);

	DVLOG(1) << "#----------------------------------------------------------------------------------#";
	DVLOG(1) << "\nVisiting Function Declaration for: " << funcDecl->getNameAsString() << std::endl
			 << "-> at location: (" << utils::location(funcDecl->getSourceRange().getBegin(), currTU->getCompiler().getSourceManager()) << "): " << std::endl
			 << "\tIsRecSubType: " << ctx.isRecSubFunc << std::endl
			 << "\tEmpty map: "    << ctx.recVarExprMap.size();

	if(!ctx.isRecSubFunc) {
		// add this type to the type graph (if not present)
		exprConv->funcDepGraph.addNode(funcDecl);
		if( VLOG_IS_ON(2) ) {
			exprConv->funcDepGraph.print( std::cout );
		}
	}

	// retrieve the strongly connected components for this type
	std::set<const FunctionDecl*>&& components = exprConv->funcDepGraph.getStronglyConnectedComponents( funcDecl );

	if( !components.empty() ) {
		// we are dealing with a recursive type
		DVLOG(1) << "Analyzing FuncDecl: " << funcDecl->getNameAsString() << std::endl
				 << "Number of components in the cycle: " << components.size();
		std::for_each(components.begin(), components.end(),
			[ ] (std::set<const FunctionDecl*>::value_type c) {
				DVLOG(2) << "\t" << c->getNameAsString( ) << "(" << c->param_size() << ")";
			}
		);

		if(!ctx.isRecSubFunc) {
			if(ctx.recVarExprMap.find(funcDecl) == ctx.recVarExprMap.end()) {
				// we create a TypeVar for each type in the mutual dependence
				core::VariablePtr&& var = builder.variable( convertType( GET_TYPE_PTR(funcDecl) ) );
				ctx.recVarExprMap.insert( std::make_pair(funcDecl, var) );
				var->addAnnotation( std::make_shared<c_info::CNameAnnotation>( funcDecl->getNameAsString() ) );
			}
		} else {
			// we expect the var name to be in currVar
			ctx.recVarExprMap.insert(std::make_pair(funcDecl, ctx.currVar));
		}

		// when a subtype is resolved we expect to already have these variables in the map
		if(!ctx.isRecSubFunc) {
			std::for_each(components.begin(), components.end(),
				[ this ] (std::set<const FunctionDecl*>::value_type fd) {

					// we count how many variables in the map refers to overloaded versions of the same function
					// this can happen when a function get overloaded and the cycle of recursion can happen between
					// the overloaded version, we need unique variable for each version of the function

					if(this->ctx.recVarExprMap.find(fd) == this->ctx.recVarExprMap.end()) {
						core::VariablePtr&& var = this->builder.variable( this->convertType(GET_TYPE_PTR(fd)) );
						this->ctx.recVarExprMap.insert( std::make_pair(fd, var ) );

						// if the function belonging to this recursion needs to access global variables
						// we create the variable that will be used in the caputre list
						if(this->ctx.globalFuncMap.find(fd) != this->ctx.globalFuncMap.end()) {
							assert(this->ctx.funcGlobalCaptureMap.find(fd) == this->ctx.funcGlobalCaptureMap.end());
							core::VariablePtr&& captVar = this->builder.variable( builder.refType(this->ctx.globalStructType) );
							this->ctx.funcGlobalCaptureMap.insert( std::make_pair(fd, captVar) );
						}
						var->addAnnotation( std::make_shared<c_info::CNameAnnotation>( fd->getNameAsString() ) );
					}
				}
			);
		}
		if( VLOG_IS_ON(2) ) {
			DVLOG(2) << "MAP: ";
			std::for_each(ctx.recVarExprMap.begin(), ctx.recVarExprMap.end(),
				[] (ConversionContext::RecVarExprMap::value_type c) {
					DVLOG(2) << "\t" << c.first->getNameAsString() << "[" << c.first << "]";
				}
			);
		}
	}

	vector<core::VariablePtr> params;
	std::for_each(funcDecl->param_begin(), funcDecl->param_end(),
		[ &params, this ] (ParmVarDecl* currParam) {
			params.push_back( core::dynamic_pointer_cast<const core::Variable>(this->lookUpVariable(currParam)) );
		}
	);

	// before resolving the body we have to set the currGlobalVar accordingly depending if
	// this function will use the global struct or not
	core::Lambda::CaptureList captureList;
	core::VariablePtr parentGlobalVar = ctx.currGlobalVar;
	if(isEntryPoint) {
		ctx.currGlobalVar = ctx.globalVar;
	} else if(ctx.globalFuncMap.find(funcDecl) != ctx.globalFuncMap.end()) {
		auto fit = ctx.funcGlobalCaptureMap.find(funcDecl);
		if(fit == ctx.funcGlobalCaptureMap.end()) {
			// declare a new variable that will be used to hold a reference to the global data stucture
			core::VariablePtr&& var = builder.variable( builder.refType(ctx.globalStructType) );
			fit = ctx.funcGlobalCaptureMap.insert( std::make_pair(funcDecl, var) ).first;
		}
		captureList.push_back( fit->second );
		ctx.currGlobalVar = fit->second;
	}

	// this lambda is not yet in the map, we need to create it and add it to the cache
	assert(!ctx.isResolvingRecFuncBody && "~~~ Something odd happened, you are allowed by all means to blame Simone ~~~");
	if(!components.empty())
		ctx.isResolvingRecFuncBody = true;
	core::StatementPtr&& body = convertStmt( funcDecl->getBody() );
	// if any of the parameters of this function has been marked as needRef,
	// we need to add a declaration just before the body of this function
	vector<core::StatementPtr> decls;
	std::for_each(params.begin(), params.end(),
		[ &decls, &body, this ] (core::VariablePtr currParam) {
			auto fit = this->ctx.needRef.find(currParam);
			if(fit != this->ctx.needRef.end()) {
				decls.push_back( this->builder.declarationStmt(fit->second,
					this->builder.callExpr( fit->second->getType(), core::lang::OP_REF_VAR_PTR, toVector<core::ExpressionPtr>( fit->first )) // ref.var
				));
				// replace this parameter in the body
				// example:
				// int f(int a) {
				//    for (...) {
				//	     x = a; <- if all the occurencies of a will not be replaced the semantics of the code will not be preserved
				//	     a = i;
				//    }
				// }
				// as the variable can olny appear in the RHS of expression, we have to sobstitute it with
				// its dereference
				body = core::dynamic_pointer_cast<const core::Statement>(core::transform::replaceAll(this->builder.getNodeManager(), body,
						fit->first, this->tryDeref(fit->second), true));
				assert(body);
			}
		}
	);
	// if we introduce new decls we have to introduce them just before the body of the function
	if(!decls.empty()) {
		// push the old body
		decls.push_back(body);
		body = builder.compoundStmt(decls);
	}
	ctx.isResolvingRecFuncBody = false;

	// ADD THE GLOBALS
	if(isEntryPoint && ctx.globalVar) {
		core::CompoundStmtPtr&& compStmt = core::dynamic_pointer_cast<const core::CompoundStmt>(body);
		assert(compStmt);
		assert(ctx.globalVar && ctx.globalStructExpr);

		std::vector<core::StatementPtr> stmts;
		stmts.push_back( builder.declarationStmt(ctx.globalVar,
				builder.callExpr( builder.refType(ctx.globalStructType), core::lang::OP_REF_VAR_PTR, toVector<core::ExpressionPtr>( ctx.globalStructExpr ) )) );
		std::copy(compStmt->getStatements().begin(), compStmt->getStatements().end(), std::back_inserter(stmts));
		body = builder.compoundStmt(stmts);
	}

	core::TypePtr convertedType = convertType( GET_TYPE_PTR(funcDecl) );
	assert(convertedType->getNodeType() == core::NT_FunctionType && "Converted type has to be a function type!");
	core::FunctionTypePtr funcType = core::static_pointer_cast<const core::FunctionType>(convertedType);

	// reset old global var
	ctx.currGlobalVar = parentGlobalVar;

	if( components.empty() ) {
		core::ExpressionPtr&& retLambdaExpr = builder.lambdaExpr( funcType, captureList, params, body);
		attachFuncAnnotations(retLambdaExpr, funcDecl);
		// Adding the lambda function to the list of converted functions
		ctx.lambdaExprCache.insert( std::make_pair(funcDecl, retLambdaExpr) );
		return retLambdaExpr;
	}

	core::LambdaPtr&& retLambdaNode = builder.lambda( funcType, captureList, params, body );
	// this is a recurive function call
	if(ctx.isRecSubFunc) {
		// if we are visiting a nested recursive type it means someone else will take care
		// of building the rectype node, we just return an intermediate type
		return retLambdaNode;
	}

	// we have to create a recursive type
	ConversionContext::RecVarExprMap::const_iterator tit = ctx.recVarExprMap.find(funcDecl);
	assert(tit != ctx.recVarExprMap.end() && "Recursive function has not VarExpr associated to himself");
	core::VariablePtr recVarRef = tit->second;

	core::LambdaDefinition::Definitions definitions;
	definitions.insert( std::make_pair(recVarRef, retLambdaNode) );

	// We start building the recursive type. In order to avoid loop the visitor
	// we have to change its behaviour and let him returns temporarely types
	// when a sub recursive type is visited.
	ctx.isRecSubFunc = true;

	std::for_each(components.begin(), components.end(),
		[ this, &definitions ] (std::set<const FunctionDecl*>::value_type fd) {

			//Visual Studios 2010 fix: full namespace
			insieme::frontend::conversion::ConversionFactory::ConversionContext::RecVarExprMap::const_iterator tit = this->ctx.recVarExprMap.find(fd);
			assert(tit != this->ctx.recVarExprMap.end() && "Recursive function has no TypeVar associated");
			this->ctx.currVar = tit->second;

			// we remove the variable from the list in order to fool the solver,
			// in this way it will create a descriptor for this type (and he will not return the TypeVar
			// associated with this recursive type). This behaviour is enabled only when the isRecSubType
			// flag is true
			this->ctx.recVarExprMap.erase(fd);
			core::LambdaPtr&& lambda = core::dynamic_pointer_cast<const core::Lambda>(this->convertFunctionDecl(fd));
			assert(lambda && "Resolution of sub recursive lambda yield a wrong result");
			definitions.insert( std::make_pair(this->ctx.currVar, lambda) );

			// reinsert the TypeVar in the map in order to solve the other recursive types
			this->ctx.recVarExprMap.insert( std::make_pair(fd, this->ctx.currVar) );
			this->ctx.currVar = NULL;
		}
	);
	// we reset the behavior of the solver
	ctx.isRecSubFunc = false;
	// the map is also erased so visiting a second type of the mutual cycle will yield a correct result
	// ctx->recVarExprMap.clear();

	core::LambdaDefinitionPtr&& definition = builder.lambdaDefinition(definitions);
	core::LambdaExprPtr&& retLambdaExpr = builder.lambdaExpr(recVarRef, definition);

	// Adding the lambda function to the list of converted functions
	ctx.lambdaExprCache.insert( std::make_pair(funcDecl, retLambdaExpr) );
	// we also need to cache all the other recursive definition, so when we will resolve
	// another function in the recursion we will not repeat the process again
	std::for_each(components.begin(), components.end(),
		[ this, &definition ] (std::set<const FunctionDecl*>::value_type fd) {
			auto fit = this->ctx.recVarExprMap.find(fd);
			assert(fit != this->ctx.recVarExprMap.end());
			core::ExpressionPtr&& func = builder.lambdaExpr(fit->second, definition);
			ctx.lambdaExprCache.insert( std::make_pair(fd, func) );

			this->attachFuncAnnotations(func, fd);
		}
	);
	attachFuncAnnotations(retLambdaExpr, funcDecl);
	return retLambdaExpr;
}

} // End conversion namespace
} // End frontend namespace
} // End insieme namespace
