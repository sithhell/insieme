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

#include "insieme/frontend/convert.h"
#include "insieme/frontend/utils/ir_cast.h"
#include "insieme/frontend/utils/debug.h"
#include "insieme/frontend/utils/source_locations.h"

#include "insieme/utils/logging.h"
#include "insieme/utils/unused.h"

#include "insieme/core/ir_expressions.h"
#include "insieme/core/ir_types.h"
#include "insieme/core/ir_builder.h"

#include "insieme/core/lang/basic.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/lang/complex_extension.h"
#include "insieme/core/lang/enum_extension.h"

#include "insieme/core/encoder/lists.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/ir++_utils.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"

#include <boost/regex.hpp>

// defines which are needed by LLVM
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#include <clang/AST/Expr.h>
#pragma GCC diagnostic pop

#include "insieme/frontend/utils/macros.h"


using namespace insieme;
using namespace insieme::frontend::utils;

namespace {

	static const boost::regex numberFilter  ("([0-9]+)(\\.[0-9]+)?([ufl]*)");
	static const boost::regex precissionFilter  ("([0-9]|([0-9]*\\.[0-9]+))([ufl]*)");
	static const boost::regex zerosFilter  ("\\.[0]+");
	static const boost::regex zeroValueFilter  ("([0]+)(\\.[0]+)?([ufl]*)");

	core::ExpressionPtr castLiteral(const core::LiteralPtr lit, const core::TypePtr targetTy){

		std::string old = lit->getStringValue();

		core::IRBuilder builder( targetTy->getNodeManager() );
		const core::lang::BasicGenerator& gen = builder.getLangBasic();
		std::string res;
		boost::cmatch what;

		////////////////////////////////////////
		// CAST TO BOOLEAN
		if (gen.isBool(targetTy) ){
			if (gen.isChar(lit->getType())) throw std::exception();
			if (boost::regex_match(old, zeroValueFilter)) res.assign("false");
			else res.assign("true");
		}

		////////////////////////////////////////
		// CAST TO CHAR
		if (gen.isChar(targetTy) ){
			// do not rebuild the literal, might be a nightmare, just build a cast
			throw std::exception();
		}

		// behind this point  is needed to be a number
		if (!boost::regex_match(old, numberFilter)){
			throw std::exception();
		}

		// cleanup precission
		if(boost::regex_match(old.c_str(), what, precissionFilter)){
			// what[1] contains the number
			// what[2] contains the precission markers
			old.assign(what[1].first, what[1].second);
		}

		////////////////////////////////////////
		// CAST TO INT and UINT
		if (gen.isInt(targetTy) || gen.isUnsignedInt(targetTy)){
			// remove any decimal
			if(boost::regex_match(old.c_str(), what, numberFilter)){
				// what[0] contains the whole string
				// what[1] contains the integer part
				// what[2] contains the decimals
				res.assign(what[1].first, what[1].second);
			}
			else {
					assert(false && "something wrong modifying literals");
			}
			if (gen.isUnsignedInt(targetTy)){
				// append u
				res.append("u");
			}
			if (gen.isInt8(targetTy) || gen.isUInt8(targetTy)){
				res.append("l");
			}
			if (gen.isInt16(targetTy) || gen.isUInt16(targetTy)){
				res.append("ll");
			}
		}

		////////////////////////////////////////
		// CAST TO REAL
		if (gen.isReal(targetTy)){
			// make sure has decimal point

			if(boost::regex_match(old.c_str(), what, numberFilter)){
				res.assign(what[1].first, what[1].second);
				if (what[2].first == what[0].second){
					//no point
					res.append(".0");
				}
				else{
					// if only zeros, clean them
					std::string zeros (what[2].first, what[2].second);
					if (boost::regex_match(zeros, zerosFilter))
						res.append(".0");
					else
						res.append(zeros);
				}
			}
			else {
					assert(false && "something wrong modifying literals");
			}

			if (gen.isReal4(targetTy)){
				//append f
				res.append("f");
			}
		}
		return builder.literal (targetTy, res);
	}

	//FIXME: getting-shit-done-solution -- code duplication with getCArrayElemRef in
	//expr_converter.cpp
	core::ExpressionPtr getCArrayElemRef(const core::IRBuilder& builder, const core::ExpressionPtr& expr) {
		const core::TypePtr& exprTy = expr->getType();
		if (exprTy->getNodeType() == core::NT_RefType) {
			const core::TypePtr& subTy = GET_REF_ELEM_TYPE(exprTy);

			if (subTy->getNodeType() == core::NT_VectorType || subTy->getNodeType() == core::NT_ArrayType) {
				core::TypePtr elemTy = core::static_pointer_cast<const core::SingleElementType>(subTy)->getElementType();

				return builder.callExpr(
						builder.refType(elemTy),
						(subTy->getNodeType() == core::NT_VectorType ?
								builder.getLangBasic().getVectorRefElem() : builder.getLangBasic().getArrayRefElem1D()),
						expr, builder.uintLit(0));
			}
		}
		return expr;
	}
}

namespace insieme {
namespace frontend {
namespace utils {



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FIXME: we can do this in a smarter way
std::size_t getPrecission(const core::TypePtr& type, const core::lang::BasicGenerator& gen){

	if (gen.isReal(type)){
		if 		(gen.isReal4(type)) return 4;
		else if (gen.isReal8(type)) return 8;
		else if (gen.isReal16(type)) return 16;
		else if (gen.isFloat(type)) return 4;
		else if (gen.isDouble(type)) return 8;
		else if (gen.isLongDouble(type)) return 16;
	}
	else if (gen.isSignedInt(type)){
		if 		(gen.isInt1(type)) return 1;
		else if (gen.isInt2(type)) return 2;
		else if (gen.isInt4(type)) return 4;
		else if (gen.isInt8(type)) return 8;
		else if (gen.isInt16(type))return 16;
	}
	else if (gen.isUnsignedInt(type)){
		if 		(gen.isUInt1(type)) return 1;
		else if (gen.isUInt2(type)) return 2;
		else if (gen.isUInt4(type)) return 4;
		else if (gen.isUInt8(type)) return 8;
		else if (gen.isUInt16(type))return 16;
	}
	else if (gen.isWChar(type)){
		if 		(gen.isWChar16(type)) return 16;
		else if (gen.isWChar32(type)) return 32;
	}
	else if (gen.isBool(type) || gen.isChar(type))
		return 1;
    else if (type.getNodeManager().getLangExtension<core::lang::EnumExtension>().isEnumType(type))
        return 4;


	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::ExpressionPtr castScalar(const core::TypePtr& trgTy, core::ExpressionPtr expr){
	core::TypePtr exprTy = expr->getType();
	core::TypePtr targetTy = trgTy;
	core::IRBuilder builder( exprTy->getNodeManager() );
	const core::lang::BasicGenerator& gen = builder.getLangBasic();
	core::NodeManager& mgr = exprTy.getNodeManager();

	//std::cout << "========= SCALAR CAST =====================" <<std::endl;
	//std::cout << "Expr: " << expr << " : " << expr->getType() << std::endl;
	//std::cout << "target Type: " << targetTy << std::endl;

	// check if casting to cpp ref, rightside values are assigned to refs in clang without any
	// conversion, because a right side is a ref and viceversa. this is invisible to us, we need to
	// handle it carefully
	if (core::analysis::isAnyCppRef(targetTy)) {
		return expr;
	}

	bool isLongLong = false;

	if (core::analysis::isLongLong (targetTy) && core::analysis::isLongLong(expr->getType())){
		if (core::analysis::isSignedLongLong(targetTy) == core::analysis::isSignedLongLong(expr->getType())){
			return expr;
		}
		else{
			return core::analysis::castBetweenLongLong(expr);
		}
	}

	// casts from long to longlong and long
	if (core::analysis::isLongLong (targetTy)){
		isLongLong = true;
		if (core::analysis::isSignedLongLong (targetTy))
			targetTy = gen.getInt8();
		else
			targetTy = gen.getUInt8();

	}

	// cast from long long
	if (core::analysis::isLongLong (exprTy)){
		expr = core::analysis::castFromLongLong( expr);
		exprTy = expr->getType();
	}

	auto lastStep = [&isLongLong, &gen] (const core::ExpressionPtr& expr) -> core::ExpressionPtr {
		if (isLongLong)
			return core::analysis::castToLongLong(expr, gen.isSignedInt(expr->getType()));
		else
			return expr;
	};

	// is this the cast of a literal: to simplify code we'll return
	// a literal of the spected type
	if (expr->getNodeType() == core::NT_Literal){
		try{
			return lastStep(castLiteral ( expr.as<core::LiteralPtr>(), targetTy));
		}catch (std::exception& e){
			// literal upgrade not supported, continue with regular cast
		}
	}


   	unsigned char code;
	// identify source type, to write right cast
	if (gen.isSignedInt (exprTy)) 	code = 1;
	if (gen.isUnsignedInt (exprTy)) code = 2;
	if (gen.isReal (exprTy))		code = 3;
	if (gen.isChar (exprTy))		code = 4;
	if (gen.isBool (exprTy))		code = 5;
    if (gen.isWChar(exprTy))		code = 6;
    if (mgr.getLangExtension<core::lang::EnumExtension>().isEnumType(exprTy)) code = 7;

	// identify target type
	if (gen.isSignedInt (targetTy)) 	code += 10;
	if (gen.isUnsignedInt (targetTy)) 	code += 20;
	if (gen.isReal (targetTy))			code += 30;
	if (gen.isChar (targetTy))			code += 40;
	if (gen.isBool (targetTy))			code += 50;
	if (gen.isWChar(targetTy))			code += 60;
	if (mgr.getLangExtension<core::lang::EnumExtension>().isEnumType(targetTy)) code += 70;


	auto doCast = [&](const core::ExpressionPtr& op,  const core::ExpressionPtr& expr, std::size_t precision) -> core::ExpressionPtr{
		if (precision) {
			core::ExpressionList args;
			args.push_back(expr);
			args.push_back(builder.getIntParamLiteral(precision));
			return builder.callExpr(targetTy, op, args);

		}
		else
			return builder.callExpr(targetTy, op, expr);
	};
	core::ExpressionPtr resIr;

	std::size_t bytes    = getPrecission(targetTy, gen);
	switch(code){
		case 11:
			// only if target precission is smaller, we may have a precission loosse.
			if (bytes != getPrecission(exprTy, gen)) resIr = doCast(gen.getIntPrecisionFix(), expr, bytes);
			else resIr = expr;
			break;
        case 17:
            resIr = builder.callExpr(gen.getInt4(), mgr.getLangExtension<core::lang::EnumExtension>().getEnumElementAsInt(), expr);
			if (bytes != getPrecission(resIr->getType(), gen)) resIr = doCast(gen.getIntPrecisionFix(), resIr, bytes);
			break;
		case 22:
			if (bytes != getPrecission(exprTy, gen)) resIr = doCast(gen.getUintPrecisionFix(), expr, bytes);
			else resIr = expr;
			break;
        case 27:
            resIr = builder.callExpr(gen.getUInt4(), mgr.getLangExtension<core::lang::EnumExtension>().getEnumElementAsUInt(), expr);
			if (bytes != getPrecission(resIr->getType(), gen)) resIr = doCast(gen.getUintPrecisionFix(), resIr, bytes);
			break;
		case 33:
			if (bytes != getPrecission(exprTy, gen)) resIr = doCast(gen.getRealPrecisionFix(), expr, bytes);
			else resIr = expr;
			break;
		case 44:
		case 55: // no cast;
			// this is a cast withing the same type.
			// is a preccision adjust, if is on the same type,
			// no need to adjust anything
			resIr = expr;
			break;
		case 66:
			if (bytes != getPrecission(exprTy, gen)) resIr = doCast(gen.getWCharPrecisionFix(), expr, bytes);
			else resIr = expr;
			break;

		case 12: resIr = doCast(gen.getUnsignedToInt(), expr, bytes); break;
		case 13: resIr = doCast(gen.getRealToInt(), expr, bytes); break;
		case 14: resIr = doCast(gen.getCharToInt(), expr, bytes); break;
		case 15: resIr = doCast(gen.getBoolToInt(), expr, bytes); break;
		case 16: resIr = doCast(gen.getWCharToInt(), expr, bytes); break;

		case 21: resIr = doCast(gen.getSignedToUnsigned(), expr, bytes); break;
		case 23: resIr = doCast(gen.getRealToUnsigned(), expr, bytes); break;
		case 24: resIr = doCast(gen.getCharToUnsigned(), expr, bytes); break;
		case 25: resIr = doCast(gen.getBoolToUnsigned(), expr, bytes); break;
		case 26: resIr = doCast(gen.getWCharToUnsigned(), expr, bytes); break;

		case 31: resIr = doCast(gen.getSignedToReal(), expr, bytes); break;
		case 32: resIr = doCast(gen.getUnsignedToReal(), expr, bytes); break;
		case 34: resIr = doCast(gen.getCharToReal(), expr, bytes); break;
		case 35: resIr = doCast(gen.getBoolToReal(), expr, bytes); break;

		case 41: resIr = doCast(gen.getSignedToChar(), expr, 0);   break;
		case 42: resIr = doCast(gen.getUnsignedToChar(), expr, 0); break;
		case 43: resIr = doCast(gen.getRealToChar(), expr, 0);     break;
		case 45: resIr = doCast(gen.getBoolToChar(), expr, 0);     break;

		case 51: resIr = doCast(gen.getSignedToBool(), expr, 0);   break;
		case 52: resIr = doCast(gen.getUnsignedToBool(), expr, 0); break;
		case 53: resIr = doCast(gen.getRealToBool(), expr, 0);     break;
		case 54: resIr = doCast(gen.getCharToBool(), expr, 0);     break;

		case 57: resIr = doCast(mgr.getLangExtension<core::lang::EnumExtension>().getEnumElementAsBool(), expr, 0); break;

		case 61: resIr = doCast(gen.getSignedToWChar(), expr, 0); break;
		case 62: resIr = doCast(gen.getUnsignedToWChar(), expr, 0); break;
		case 63: resIr = doCast(gen.getRealToWChar(), expr, 0); break;
		case 64: resIr = doCast(gen.getCharToWChar(), expr, 0); break;
		case 65: resIr = doCast(gen.getBoolToWChar(), expr, 0); break;

		case 71: resIr = builder.deref(builder.callExpr(builder.refType(targetTy), gen.getRefReinterpret(),
                                     builder.refVar(expr), builder.getTypeLiteral(targetTy))); break;
		case 72: resIr = builder.deref(builder.callExpr(builder.refType(targetTy), gen.getRefReinterpret(),
                                     builder.refVar(expr), builder.getTypeLiteral(targetTy))); break;

        case 77: resIr = expr; break;

		default:
				 std::cerr << "expr type: " << exprTy << std::endl;
				 std::cerr << "targ type: " << targetTy << std::endl;
				 std::cerr << "code: " << (int) code << std::endl;
				 assert(false && "cast not defined");
	}


	// idelayed casts from long to longlong and long
	return lastStep(resIr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::ExpressionPtr castToBool (const core::ExpressionPtr& expr){

	const core::TypePtr& exprTy = expr->getType();
	core::IRBuilder builder( exprTy->getNodeManager() );
	const core::lang::BasicGenerator& gen = builder.getLangBasic();

	if (gen.isBool(expr->getType())) return expr;

	if (isRefArray(expr->getType())) {
		return builder.callExpr(gen.getBool(), gen.getBoolLNot(), builder.callExpr(gen.getBool(), gen.getRefIsNull(), expr));
	}

	if (gen.isAnyRef(exprTy)) {
		return builder.callExpr(gen.getBool(), gen.getBoolLNot(), builder.callExpr(gen.getBool(), gen.getRefIsNull(), expr));
	}

	if( exprTy.isa<core::FunctionTypePtr>()) {
		return builder.callExpr(gen.getBool(), gen.getGenNe(), expr, builder.getZero(exprTy));
	}

	if (core::analysis::isLongLong (exprTy)){
	    return castScalar (gen.getBool(),core::analysis::castFromLongLong( expr));
	}

	if (!gen.isInt(expr->getType())  && !gen.isReal(expr->getType()) && !gen.isChar(expr->getType())){
		dumpDetail(expr);
		std::cout << "****" << std::endl;
		dumpDetail(expr->getType());
		assert(false && "this type can not be converted now to bool. implement it! ");
	}

	return castScalar (gen.getBool(), expr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Takes a clang::CastExpr, converts its subExpr into IR and wraps it with the necessary IR casts
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
core::ExpressionPtr performClangCastOnIR (insieme::frontend::conversion::Converter& convFact,
										  const clang::CastExpr* castExpr){
	const core::IRBuilder& builder = convFact.getIRBuilder();
	const core::lang::BasicGenerator& gen = builder.getLangBasic();
	core::NodeManager& mgr = convFact.getNodeManager();

	core::ExpressionPtr expr = convFact.convertExpr(castExpr->getSubExpr());
	core::TypePtr  targetTy = convFact.convertType(GET_TYPE_PTR(castExpr));

	core::TypePtr&& exprTy = expr->getType();

	//	if (VLOG_IS_ON(2)){
	//		VLOG(2) << "####### Expr: #######" ;
	//		VLOG(2) << (expr);
	//		VLOG(2) << "####### Expr Type: #######" ;
	//		VLOG(2) << (exprTy);
	//		VLOG(2) << "####### cast Type: #######" ;
	//		VLOG(2) << (targetTy);
	//		VLOG(2)  << "####### clang: #######" << std::endl;
	//		castExpr->dump();
	//	}

	// it might be that the types are already fixed:
	// like LtoR in arrays, they will allways be a ref<...>
	if (*exprTy == *targetTy)
		return expr;

	// handle implicit casts according to their kind
	switch (castExpr->getCastKind()) {

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_LValueToRValue 	:
		// A conversion which causes the extraction of an r-value from the operand gl-value.
		// The result of an r-value conversion is always unqualified.
		//
		// IR: this is the same as out ref deref ref<a'> -> a'
		{
			// we use by value a member accessor. we have a better operation for this
			// instead of derefing the memberRef
			//  refElem   (ref<owner>, elemName) -> ref<member>   => this is composite ref
			//  membAcces ( owner, elemName) -> member            => uses read-only, returns value
			if(core::CallExprPtr call = expr.isa<core::CallExprPtr>()){
				if (core::analysis::isCallOf(call, gen.getCompositeRefElem()) &&
						(!core::analysis::isCallOf(call, mgr.getLangExtension<core::lang::IRppExtensions>().getRefCppToIR()) &&
						 !core::analysis::isCallOf(call, mgr.getLangExtension<core::lang::IRppExtensions>().getRefConstCppToIR()))){
						expr= builder.callExpr (gen.getCompositeMemberAccess(),
						builder.deref (call[0]), call[1], builder.getTypeLiteral(targetTy));
					}
				// TODO: we can do something similar and turn vector ref elem into vectorSubscript
				//else if (core::analysis::isCallOf(call, gen.getVectorRefElem())) {
				else if (expr->getType().isa<core::RefTypePtr>()){
					expr = builder.deref(expr);
				}
			}
			else if (expr->getType().isa<core::RefTypePtr>()){
				expr = builder.deref(expr);
			}

			// this is CppRef -> ref
			if (core::analysis::isAnyCppRef(expr->getType())){
				expr = builder.deref(builder.toIRRef(expr));
			}

			return expr;
			break;
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////

		case clang::CK_IntegralCast 	:
		// A cast between integral types (other than to boolean). Variously a bitcast, a truncation,
		// a sign-extension, or a zero-extension. long l = 5; (unsigned) i
		//IR: convert to int or uint
		case clang::CK_IntegralToBoolean 	:
		// Integral to boolean. A check against zero. (bool) i
		case clang::CK_IntegralToFloating 	:
		// Integral to floating point. float f = i;
		case clang::CK_FloatingToIntegral 	:
		// Floating point to integral. Rounds towards zero, discarding any fractional component.
		// (int) f
		case clang::CK_FloatingToBoolean 	:
		// Floating point to boolean. (bool) f
		case clang::CK_FloatingCast 	:
		// Casting between floating types of different size. (double) f (float) ld
		{
		    assert(builder.getLangBasic().isPrimitive(expr->getType())
                || mgr.getLangExtension<core::lang::EnumExtension>().isEnumType(expr->getType())
				|| core::analysis::isLongLong (expr->getType()));
			assert(builder.getLangBasic().isPrimitive(targetTy)
                || mgr.getLangExtension<core::lang::EnumExtension>().isEnumType(targetTy)
				|| core::analysis::isLongLong (targetTy));
			return castScalar( targetTy, expr);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_NoOp:
		// A conversion which does not affect the type other than (possibly) adding qualifiers. i
		// int -> int char** -> const char * const *
		{
			if (core::analysis::isCppRef(exprTy)){
				return builder.callExpr (mgr.getLangExtension<core::lang::IRppExtensions>().getRefCppToConstCpp(), expr);
			}

			// types equality has been already checked, if is is a NoOp is because clang identifies
			// this as the same type. but we might intepret it in a diff way. (ex, char literals are
			// int for clang, we build a char type
			if (gen.isPrimitive(exprTy) ) {
				return castScalar(targetTy, expr);
			}
			else {
				return expr;
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_ArrayToPointerDecay 	:
		// Array to pointer decay. int[10] -> int* char[5][6] -> char(*)[6]
		{
			// if inner expression is not ref.... it might be a compound initializer
			if (!IS_IR_REF(exprTy)){
				expr = builder.callExpr(gen.getRefVar(),expr);
			}

			if (core::analysis::isAnyCppRef(exprTy)){
				expr = core::analysis::unwrapCppRef(expr);
			}

			return builder.callExpr(gen.getRefVectorToRefArray(), expr);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_BitCast 	:
		// A conversion which causes a bit pattern of one type to be reinterpreted as a bit pattern
		//of another type.
		//Generally the operands must have equivalent size and unrelated types.  The pointer conversion
		//char* -> int* is a bitcast. A conversion from any pointer type to a C pointer type is a bitcast unless
		//it's actually BaseToDerived or DerivedToBase. A conversion to a block pointer or ObjC pointer type is a
		//bitcast only if the operand has the same type kind; otherwise, it's one of the specialized casts below.
		//Vector coercions are bitcasts.
		{
			// char* -> const char* is a bitcast in clang, and not a Noop, but we drop qualifiers
			if (*targetTy == *exprTy) return expr;

			// cast to void*
			if (gen.isAnyRef(targetTy)) {
				//return builder.callExpr(builder.getLangBasic().getRefToAnyRef(), expr); }
				return expr;
			}

			// is a cast of Null to another pointer type:
			// we rebuild null
			if (*expr == *builder.literal(expr->getType(), "0") || gen.isRefNull(expr)) {
				return builder.getZero(targetTy);
			}

			// cast from void*
			if (gen.isAnyRef(exprTy)) {
				core::TypePtr elementType = core::analysis::getReferencedType(targetTy);
				return builder.callExpr(targetTy, gen.getRefReinterpret(),
										expr, builder.getTypeLiteral(elementType));
			}

			// otherwhise, we just reinterpret
			core::ExpressionPtr innerExpr = expr;
			if (gen.isRefDeref(expr)){
				//clang does LtoR always, but we want refs in the cast. if there is a deref in the inner expression remove it
				innerExpr = expr.as<core::LambdaExprPtr>()->getParameterList()[0];
			}
			return builder.callExpr(targetTy, gen.getRefReinterpret(),
									innerExpr, builder.getTypeLiteral(GET_REF_ELEM_TYPE(targetTy)));
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_VectorSplat 	:
		// A conversion from an arithmetic type to a vector of that element type. Fills all elements
		//("splats") with the source value. __attribute__((ext_vector_type(4))) int v = 5;
		{
			return builder.callExpr(gen.getVectorInitUniform(),
					expr,
					builder.getIntTypeParamLiteral(targetTy.as<core::VectorTypePtr>()->getSize())
				);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_IntegralToPointer 	:
		//Integral to pointer. A special kind of reinterpreting conversion. Applies to normal,
		//ObjC, and block pointers. (char*) 0x1001aab0 reinterpret_cast<int*>(0)
		{
			// is a cast of Null to another pointer type:
			// we rebuild null
			if (*expr == *builder.literal(expr->getType(), "0") || gen.isRefNull(expr)) {
				return builder.getZero(targetTy);
			}
			else{
				assert(targetTy.isa<core::RefTypePtr>());
				core::TypePtr elemTy = targetTy.as<core::RefTypePtr>()->getElementType();
				return builder.callExpr(targetTy, gen.getIntToRef(), toVector(expr, builder.getTypeLiteral(elemTy)));
			}
			break;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_ConstructorConversion 	:
		//Conversion by constructor. struct A { A(int); }; A a = A(10);
		{
			// this should be handled by backend compiler
			// http://stackoverflow.com/questions/1384007/conversion-constructor-vs-conversion-operator-precedence
			return expr;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_PointerToBoolean 	:
		//case clang::CK_PointerToBoolean - Pointer to boolean conversion. A check against null. Applies to normal, ObjC,
		// and block pointers.
			return builder.callExpr(gen.getBoolLNot(), builder.callExpr( gen.getBool(), gen.getRefIsNull(), expr ));

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_FloatingRealToComplex 	:
		case clang::CK_IntegralRealToComplex 	:
			return builder.callExpr(mgr.getLangExtension<core::lang::ComplexExtension>().getConstantToComplex(), expr);
		/*A conversion of a floating point real to a floating point complex of the original type. Injects the value as the
		* real component with a zero imaginary component. float -> _Complex float.
		* */
		/*Converts from an integral real to an integral complex whose element type matches the source. Injects the value as the
		* real component with a zero imaginary component. long -> _Complex long.
		* */

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_FloatingComplexCast 	:
		case clang::CK_FloatingComplexToIntegralComplex 	:
		case clang::CK_IntegralComplexCast 	:
		case clang::CK_IntegralComplexToFloatingComplex 	:
			return mgr.getLangExtension<core::lang::ComplexExtension>().castComplexToComplex(expr, targetTy);
		/*Converts between different floating point complex types. _Complex float -> _Complex double.
		* */
		/*Converts from a floating complex to an integral complex. _Complex float -> _Complex int.
		* */
		/*Converts between different integral complex types. _Complex char -> _Complex long long _Complex unsigned int ->
		* _Complex signed int.
		* */
		/*Converts from an integral complex to a floating complex. _Complex unsigned -> _Complex float.
		* */

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_FloatingComplexToReal 	:
		case clang::CK_IntegralComplexToReal 	:
			return mgr.getLangExtension<core::lang::ComplexExtension>().getReal(expr);
		/*Converts a floating point complex to floating point real of the source's element type. Just discards the imaginary
		* component. _Complex long double -> long double.
		* */
		/*Converts an integral complex to an integral real of the source's element type by discarding the imaginary component.
		* _Complex short -> short.
		* */


		/////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_IntegralComplexToBoolean 	:
		case clang::CK_FloatingComplexToBoolean 	:
		    /*Converts a complex to bool by comparing against 0+0i.
		* */
			return mgr.getLangExtension<core::lang::ComplexExtension>().castComplexToBool(expr);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_LValueBitCast 	:
		/* case clang::CK_LValueBitCast - A conversion which reinterprets the address of an l-value as an l-value of a different
		* kind. Used for reinterpret_casts of l-value expressions to reference types. bool b; reinterpret_cast<char&>(b) = 'a';
		* */
		{
			//if we have a cpp ref we have to unwrap the expression
			if(core::analysis::isAnyCppRef(expr->getType())) {
				expr = builder.toIRRef(expr);
			}
			//the target type is a ref type because lvalue
			//bitcasts look like reinterpret_cast<type&>(x)
			targetTy = builder.refType(targetTy);
			core::CallExprPtr newExpr = builder.callExpr(targetTy, gen.getRefReinterpret(),
                                                         expr, builder.getTypeLiteral(GET_REF_ELEM_TYPE(targetTy)));
			//wrap it as cpp ref
			return builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getRefIRToCpp(), newExpr);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_NullToPointer 	:
		// Null pointer constant to pointer, ObjC pointer, or block pointer. (void*) 0;
		{
			if(gen.isAnyRef(targetTy)) { return gen.getRefNull(); }

			//if( targetTy.isa<core::RefTypePtr>() && core::analysis::getReferencedType(targetTy).isa<core::FunctionTypePtr>() ) {
			if( targetTy.isa<core::FunctionTypePtr>()){
				return builder.callExpr(targetTy, gen.getNullFunc(), builder.getTypeLiteral(targetTy));
			}

			// cast NULL to anything else
			if( ((targetTy->getNodeType() == core::NT_RefType)) && (*expr == *builder.literal(expr->getType(), "0") || gen.isRefNull(expr))) {
				return builder.getZero(targetTy);
			}
			else{

				// it might be that is a reinterpret cast already, we can avoid chaining
				if (core::analysis::isCallOf(expr, gen.getRefReinterpret())){
					expr = expr.as<core::CallExprPtr>()[0];
				}

				// it might be a function type e.g. fun(void *(**) (size_t)) {} called with fun(__null)
				if(core::dynamic_pointer_cast<const core::FunctionType>(targetTy)) {
					return builder.getZero(targetTy);
				}

				core::TypePtr elementType = core::analysis::getReferencedType(targetTy);
				assert(elementType && "cannot build ref reinterpret without a type");
				return builder.callExpr(targetTy, gen.getRefReinterpret(),
										expr, builder.getTypeLiteral(elementType));
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_MemberPointerToBoolean 	:
		/*case clang::CK_MemberPointerToBoolean - Member pointer to boolean. A check against the null member pointer.
		* */
		    return builder.callExpr(gen.getBoolLNot(), builder.callExpr( gen.getBool(), gen.getRefIsNull(), expr ));

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		//  PARTIALY IMPLEMENTED
		//////////////////////////////////////////////////////////////////////////////////////////////////////////

		/////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_ToVoid 	:
		//Cast to void, discarding the computed value. (void) malloc(2048)
		{
			// this cast ignores inner expression, is not very usual, but might show off when dealing
			// with null pointer.
			if (gen.isUnit(targetTy)) { return gen.getUnitConstant(); }
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_UncheckedDerivedToBase:
		case clang::CK_DerivedToBase:
		//A conversion from a C++ class pointer to a base class pointer. A *a = new B();
		{
			// TODO: do we need to check if is pointerType?
			// in case of pointer, the inner expression is modeled as ref< array < C, 1> >
			// it is needed to deref the first element
			expr = getCArrayElemRef(builder, expr);

			// unwrap CppRef if CppRef
			if (core::analysis::isAnyCppRef(exprTy)){
				expr = core::analysis::unwrapCppRef(expr);
			}

			clang::CastExpr::path_const_iterator it;
			for (it = castExpr->path_begin(); it!= castExpr->path_end(); ++it){
				core::TypePtr targetTy= convFact.convertType((*it)->getType().getTypePtr());
				//if it is no ref we have to materialize it, otherwise refParent cannot be called
				if(expr->getType()->getNodeType() != core::NT_RefType) {
					expr = builder.callExpr (mgr.getLangExtension<core::lang::IRppExtensions>().getMaterialize(), expr);
				}
				expr = builder.refParent(expr, targetTy);
			}

			if(GET_TYPE_PTR(castExpr)->isPointerType()){
				// is a pointer type -> return pointer
				expr = builder.callExpr(gen.getScalarToArray(), expr);
			}

			VLOG(2) << expr;
			return expr;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_BaseToDerived:
		//A conversion from a C++ class pointer/reference to a derived class pointer/reference. B *b = static_cast<B*>(a);
		{
			//we want to know the TYPE of static_cast<TYPE>()
			targetTy = convFact.convertType(GET_TYPE_PTR(llvm::dyn_cast<clang::ExplicitCastExpr>(castExpr)->getTypeInfoAsWritten()));
			VLOG(2) << exprTy << " " << targetTy;

			core::ExpressionPtr retIr;
			if (core::analysis::isCppRef(exprTy) && core::analysis::isCppRef(targetTy)) {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getStaticCastRefCppToRefCpp(), expr, builder.getTypeLiteral((targetTy)));
			}
			else if (core::analysis::isConstCppRef(exprTy) && core::analysis::isConstCppRef(targetTy)) {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getStaticCastConstCppToConstCpp(), expr, builder.getTypeLiteral((targetTy)));
			}
			else if (core::analysis::isCppRef(exprTy) && core::analysis::isConstCppRef(targetTy)) {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getStaticCastRefCppToConstCpp(), expr, builder.getTypeLiteral((targetTy)));
			}
			else if (	!(core::analysis::isCppRef(exprTy) || core::analysis::isConstCppRef(exprTy))
					&&  (core::analysis::isCppRef(targetTy) || core::analysis::isConstCppRef(targetTy)) ) {
				// statically casting an object to a reference

				// first wrap object in cpp_ref
				expr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getRefIRToCpp(), expr);

				//depending on targetType
				if(core::analysis::isCppRef(targetTy) ) {
					retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getStaticCastRefCppToRefCpp(), expr, builder.getTypeLiteral(targetTy));
				}
				else if(core::analysis::isConstCppRef(targetTy)) {
					retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getStaticCastRefCppToConstCpp(), expr, builder.getTypeLiteral(targetTy));
				}
			} else {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getStaticCast(), expr, builder.getTypeLiteral(GET_REF_ELEM_TYPE(targetTy)));
			}

			VLOG(2) << retIr << " " << retIr->getType();
			return retIr;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_Dynamic:
		// A C++ dynamic_cast.
		{
			//we want to know the TYPE of dynamic_cast<TYPE>()
			targetTy = convFact.convertType(GET_TYPE_PTR(llvm::dyn_cast<clang::ExplicitCastExpr>(castExpr)->getTypeInfoAsWritten()));
			VLOG(2) << exprTy << " " << targetTy;

			core::ExpressionPtr retIr;
			if (core::analysis::isCppRef(exprTy) && core::analysis::isCppRef(targetTy)) {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getDynamicCastRefCppToRefCpp(), expr, builder.getTypeLiteral((targetTy)));
			}
			else if (core::analysis::isConstCppRef(exprTy) && core::analysis::isConstCppRef(targetTy)) {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getDynamicCastConstCppToConstCpp(), expr, builder.getTypeLiteral((targetTy)));
			}
			else if (core::analysis::isCppRef(exprTy) && core::analysis::isConstCppRef(targetTy)) {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getDynamicCastRefCppToConstCpp(), expr, builder.getTypeLiteral((targetTy)));
			}
			else if (	!(core::analysis::isCppRef(exprTy) || core::analysis::isConstCppRef(exprTy))
					&&  (core::analysis::isCppRef(targetTy) || core::analysis::isConstCppRef(targetTy)) ) {
				// dynamically casting an object to a reference

				// first wrap object in cpp_ref
				expr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getRefIRToCpp(), expr);

				//depending on targetType
				if(core::analysis::isCppRef(targetTy) ) {
					retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getDynamicCastRefCppToRefCpp(), expr, builder.getTypeLiteral(targetTy));
				}
				else if(core::analysis::isConstCppRef(targetTy)) {
					retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getDynamicCastRefCppToConstCpp(), expr, builder.getTypeLiteral(targetTy));
				}
			}
			else {
				retIr = builder.callExpr(mgr.getLangExtension<core::lang::IRppExtensions>().getDynamicCast(), expr, builder.getTypeLiteral(GET_REF_ELEM_TYPE(targetTy)));
			}

			VLOG(2) << retIr << " " << retIr->getType();
			return retIr;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_PointerToIntegral :
		// CK_PointerToIntegral - Pointer to integral. A special kind of reinterpreting conversion. Applies to normal,
		// ObjC, and block pointers. (intptr_t) "help!"
		{
			return builder.callExpr(gen.getUInt8(), gen.getRefToInt(), expr);
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_FunctionToPointerDecay 	:
		// CK_FunctionToPointerDecay - Function to pointer decay. void(int) -> void(*)(int)
		{
			return expr;
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_NullToMemberPointer 	:
		/*case clang::CK_NullToMemberPointer - Null pointer constant to member pointer.
		 * int A::*mptr = 0; int (A::*fptr)(int) = nullptr;
		 */
		{
			return builder.callExpr(targetTy, gen.getNullFunc(), builder.getTypeLiteral(targetTy));
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		case clang::CK_UserDefinedConversion 	:
		/*case clang::CK_UserDefinedConversion - Conversion using a user defined type conversion function.i
		* struct A { operator int(); }; int i = int(A());
		* */
		{
			return convFact.convertExpr(castExpr->getSubExpr ());
		}

		//////////////////////////////////////////////////////////////////////////////////////////////////////////
		//  NOT IMPLEMENTED
		//////////////////////////////////////////////////////////////////////////////////////////////////////////

		case clang::CK_Dependent 	:
		/*case clang::CK_Dependent - A conversion which cannot yet be analyzed because either the expression
		* or target type is dependent. These are created only for explicit casts; dependent ASTs aren't
		* required to even approximately type-check. (T*) malloc(sizeof(T)) reinterpret_cast<intptr_t>(A<T>::alloc());
		* */

		case clang::CK_ToUnion 	:
		/*case clang::CK_ToUnion - The GCC cast-to-union extension. i
		* int -> union { int x; float y; } float -> union { int x; float y; }
		* */

		case clang::CK_BaseToDerivedMemberPointer 	:
		/*case clang::CK_BaseToDerivedMemberPointer - Member pointer in base class to member pointer in derived class.
		 * int B::*mptr = &A::member;
		* */

		case clang::CK_DerivedToBaseMemberPointer 	:
		/*case clang::CK_DerivedToBaseMemberPointer - Member pointer in derived class to member pointer in base class.
		* int A::*mptr = static_cast<int A::*>(&B::member);
		* */

		case clang::CK_ReinterpretMemberPointer 	:
		/*case clang::CK_ReinterpretMemberPointer - Reinterpret a member pointer as a different kind of member pointer.
		* C++ forbids this from crossing between function and object types, but otherwise does not restrict it.
		* However, the only operation that is permitted on a "punned" member pointer is casting it back to the original type,
		* which is required to be a lossless operation (although many ABIs do not guarantee this on all possible intermediate types).
		* */

		case clang::CK_CPointerToObjCPointerCast 	:
		/*case clang::CK_CPointerToObjCPointerCast - Casting a C pointer kind to an Objective-C pointer.
		* */

		case clang::CK_BlockPointerToObjCPointerCast 	:
		/*case clang::CK_BlockPointerToObjCPointerCast - Casting a block pointer to an ObjC pointer.
		* */

		case clang::CK_AnyPointerToBlockPointerCast 	:
		/*case clang::CK_AnyPointerToBlockPointerCast - Casting any non-block pointer to a block pointer. Block-to-block casts are bitcasts.
		* */

		case clang::CK_ObjCObjectLValueCast 	:
		/*Converting between two Objective-C object types, which can occur when performing reference binding to an Objective-C object.
		* */

		case clang::CK_ARCProduceObject 	:
		/*[ARC] Produces a retainable object pointer so that it may be consumed, e.g. by being passed to a consuming parameter.
		* Calls objc_retain.
		* */

		case clang::CK_ARCConsumeObject 	:
		/*[ARC] Consumes a retainable object pointer that has just been produced, e.g. as the return value of a retaining call.
		* Enters a cleanup to call objc_release at some indefinite time.
		* */

		case clang::CK_ARCReclaimReturnedObject 	:
		/*[ARC] Reclaim a retainable object pointer object that may have been produced and autoreleased as part of a function
		* return sequence.
		* */

		case clang::CK_ARCExtendBlockObject 	:
		/*[ARC] Causes a value of block type to be copied to the heap, if it is not already there. A number of other operations in
		* ARC cause blocks to be copied; this is for cases where that would not otherwise be guaranteed, such as when casting to a
		* non-block pointer type.
		* */

		case clang::CK_AtomicToNonAtomic 	:
		case clang::CK_NonAtomicToAtomic 	:
		case clang::CK_CopyAndAutoreleaseBlockObject 	:
		case clang::CK_BuiltinFnToFnPtr 	:
			std::cout << " \nCAST: " << castExpr->getCastKindName () << " not supported!!"<< std::endl;
			std::cout << " at location: " << frontend::utils::location(castExpr->getLocStart (), convFact.getSourceManager()) <<std::endl;
			castExpr->dump();
			assert(false);
		default:
			assert(false && "not all options listed, is this clang 3.2? maybe should upgrade Clang support");
	}

	assert(false && "control reached an invalid point!");

	return expr;

}

} // end utils namespace
} // end frontend namespace
} // end insieme namespace
