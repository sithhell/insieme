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

// FIXME this needs a cleaner solution
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "insieme/frontend/utils/functionDependencyGraph.h"
#include "insieme/frontend/utils/source_locations.h"

#include "insieme/frontend/convert.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"


namespace insieme {
namespace frontend {

namespace utils {

template<>
void DependencyGraph<const clang::FunctionDecl*>::Handle(const clang::FunctionDecl* func,
													     const DependencyGraph<const clang::FunctionDecl*>::VertexTy& v) {
	// This is potentially dangerous
	FunctionDependencyGraph& funcDepGraph = static_cast<FunctionDependencyGraph&>(*this);

	CallExprVisitor callExprVis(funcDepGraph.getIndexer(), funcDepGraph.getInterceptor());
	CallExprVisitor::CallGraph&& graph = callExprVis.getCallGraph(func);

	std::for_each(graph.begin(), graph.end(),
			[ this, v ](const clang::FunctionDecl* currFunc) {assert(currFunc); this->addNode(currFunc, &v);});
}

/*************************************************************************************************
 * CallExprVisitor 
 *************************************************************************************************/
CallExprVisitor::CallGraph CallExprVisitor::getCallGraph(const clang::FunctionDecl* func) {
	
		// FUNCTION might be intercepted
	if( interceptor.isIntercepted(func)) {
		// we don't care about interanls of intercepted function 
		VLOG(2) << "isIntercepted " << func << "("<<((void*)func)<<")";
	
		// OR a function with vistable body
	} else if (func->getBody()) {
		Visit(func->getBody());

		// OR any other case, most probably an Spetialitation 
	} else {

		std::cout << func->getQualifiedNameAsString() << std::endl;
		std::cout << "at location: " 
				  << utils::location(func->getLocStart(),
						  			 func->getTranslationUnitDecl()->getASTContext().getSourceManager()) 
				  << std::endl;

		if (func->isFunctionTemplateSpecialization ()){
			clang::FunctionDecl* f = func->getInstantiatedFromMemberFunction ();
			if(!f) f = func->getTemplateInstantiationPattern () ;
			if(!f) f = func->getClassScopeSpecializationPattern () ;
			assert(f);
			assert(f->hasBody());
		}
	}
	return callGraph;
}

void CallExprVisitor::addFunctionDecl(clang::FunctionDecl* funcDecl) {
	assert(funcDecl && "no function to index");

	const clang::FunctionDecl* def = NULL;
	// if the function has no body, we need to find the right declaration with
	// the definition in another translation unit
	if (!funcDecl->hasBody(def)) {
		// it might be a template spetialitation:

		if (funcDecl->isFunctionTemplateSpecialization ()){
			std::cout << " is a template: " << std::endl;
				def = funcDecl->getInstantiatedFromMemberFunction ();
				if(!def) def = funcDecl->getTemplateInstantiationPattern () ;
				if(!def) def = funcDecl->getClassScopeSpecializationPattern () ;
				assert(def);
				assert(def->hasBody());
		} else {
			clang::Decl* raw = indexer.getDefinitionFor (funcDecl);
			if (raw){
				def = llvm::cast<clang::FunctionDecl>(raw);
			}
		}
	}

	if (def){
		// add the funcDecl
		callGraph.insert(def);
	} else if( interceptor.isIntercepted(funcDecl)) {
		// call to an intercepted function (without body) 
		callGraph.insert(funcDecl);
	}
	else {
		//std::cout << "    no body, not intercepted: " << funcDecl->getQualifiedNameAsString() << std::endl;
	}
}

void CallExprVisitor::VisitCallExpr(clang::CallExpr* callExpr) {
	if (callExpr->getDirectCallee()) {
		if (clang::FunctionDecl * funcDecl = llvm::dyn_cast<clang::FunctionDecl>(callExpr->getDirectCallee())) {
			addFunctionDecl(funcDecl);
		}
	}
	VisitStmt(callExpr);
}

void CallExprVisitor::VisitDeclRefExpr(clang::DeclRefExpr* expr) {
	//FIXME: why is this commented??
	// if this variable is used to invoke a function (therefore is a
	// function pointer) and it has been defined here, we add a potentially
	// dependency to the current definition
	//if ( FunctionDecl* funcDecl = dyn_cast<FunctionDecl>(expr->getDecl()) ) {
	// addFunctionDecl(funcDecl);
	//}
}
	
void CallExprVisitor::VisitStmt(clang::Stmt* stmt) {
	std::for_each(stmt->child_begin(), stmt->child_end(),
			[ this ](clang::Stmt* curr) {if(curr) this->Visit(curr);});
}

void CallExprVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr* ctorExpr) {

	//assert(false && "constructor expression");

	clang::CXXConstructorDecl* constructorDecl = ctorExpr->getConstructor();
	assert(constructorDecl);

	// connects the constructor expression to the function graph
	// if constructor has no body, addFunctionDecl wont count it
	clang::FunctionDecl* fDecl = llvm::cast<clang::FunctionDecl>(constructorDecl);
	addFunctionDecl(fDecl);
	VisitStmt(ctorExpr);

	// if there is an member with an initializer in the ctor we add it to the function graph
	for (clang::CXXConstructorDecl::init_iterator iit = constructorDecl->init_begin(), iend =
			constructorDecl->init_end(); iit != iend; iit++) {
		clang::CXXCtorInitializer * initializer = *iit;

		if (initializer->isMemberInitializer()) {
			Visit(initializer->getInit());
		}
	}

	// if we construct a object there should be some kind of destructor
	// we have to add it to the function graph
	if ( clang::CXXRecordDecl* classDecl = GET_TYPE_PTR(ctorExpr)->getAsCXXRecordDecl()) {
		clang::CXXDestructorDecl* dtorDecl = classDecl->getDestructor();
		if (dtorDecl)
			addFunctionDecl(dtorDecl);
	}
}

//void CallExprVisitor::VisitCXXNewExpr(clang::CXXNewExpr* callExpr) {
	//if there is an member with an initializer in the ctor we add it to the function graph
	//if (clang::CXXConstructorDecl * constructorDecl = llvm::dyn_cast<clang::CXXConstructorDecl>(llvm::cast<clang::FunctionDecl>(callExpr->getConstructor()))) {
//		// connects the constructor expression to the function graph
//		addFunctionDecl(constructorDecl);
//		for (clang::CXXConstructorDecl::init_iterator iit = constructorDecl->init_begin(), iend =
//				constructorDecl->init_end(); iit != iend; iit++) {
//			clang::CXXCtorInitializer * initializer = *iit;
//
//			if (initializer->isMemberInitializer()) {
//				Visit(initializer->getInit());
//			}
//		}

	//VisitCXXConstructExpr(callExpr->getConstructorExp());

	//}

//	VisitStmt(callExpr);
//}

//void CallExprVisitor::VisitCXXDeleteExpr(clang::CXXDeleteExpr* callExpr) {


	//assert(false && "delete expression");

	/*
	addFunctionDecl(callExpr->getOperatorDelete());

	// if we delete a class object -> add destructor to function call
	if ( clang::CXXRecordDecl* classDecl = callExpr->getDestroyedType()->getAsCXXRecordDecl()) {
		clang::CXXDestructorDecl* dtorDecl = classDecl->getDestructor();
		addFunctionDecl(dtorDecl);
	}

	VisitStmt(callExpr);
	*/
//}

void CallExprVisitor::VisitCXXMemberCallExpr(clang::CXXMemberCallExpr* mcExpr) {
	// connects the member call expression to the function graph
	if(clang::FunctionDecl* funcDecl = llvm::dyn_cast<clang::FunctionDecl>(mcExpr->getCalleeDecl())) {
		addFunctionDecl(funcDecl);
	}

	VisitStmt(mcExpr);
}

} // end utils namespace 
} // end utils frontend 
} // end utils insieme 
