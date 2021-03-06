{-
 - Copyright (c) 2002-2017 Distributed and Parallel Systems Group,
 -                Institute of Computer Science,
 -               University of Innsbruck, Austria
 -
 - This file is part of the INSIEME Compiler and Runtime System.
 -
 - This program is free software: you can redistribute it and/or modify
 - it under the terms of the GNU General Public License as published by
 - the Free Software Foundation, either version 3 of the License, or
 - (at your option) any later version.
 -
 - This program is distributed in the hope that it will be useful,
 - but WITHOUT ANY WARRANTY; without even the implied warranty of
 - MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 - GNU General Public License for more details.
 -
 - You should have received a copy of the GNU General Public License
 - along with this program.  If not, see <http://www.gnu.org/licenses/>.
 -
 -
 - If you require different license terms for your intended use of the
 - software, e.g. for proprietary commercial or industrial use, please
 - contact us at:
 -                   insieme@dps.uibk.ac.at
 -
 - We kindly ask you to acknowledge the use of this software in any
 - publication or other disclosure of results by referring to the
 - following citation:
 -
 - H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 - T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 - for Parallel Codes, in Proc. of the Intl. Conference for High
 - Performance Computing, Networking, Storage and Analysis (SC 2012),
 - IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 -}

{-# LANGUAGE DeriveAnyClass #-}
{-# LANGUAGE DeriveGeneric #-}
{-# LANGUAGE FlexibleInstances #-}

module Insieme.Analysis.Arithmetic where

import Control.DeepSeq (NFData)
import Data.Maybe
import Data.Typeable
import Data.Hashable
import GHC.Generics (Generic)

import Insieme.Inspire (NodeAddress)
import qualified Insieme.Inspire as I
import qualified Insieme.Query as Q
import qualified Insieme.Utils.Arithmetic as Ar
import qualified Insieme.Utils.BoundSet as BSet
import Insieme.Utils.ParseInt


import Insieme.Analysis.Entities.FieldIndex
import Insieme.Analysis.Entities.SymbolicFormula
import Insieme.Analysis.Framework.Utils.OperatorHandler
import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue
import qualified Insieme.Analysis.Framework.PropertySpace.ValueTree as ValueTree
import qualified Insieme.Solver as Solver

import {-# SOURCE #-} Insieme.Analysis.Framework.Dataflow


--
-- * Arithemtic Lattice
--

newtype SymbolicFormulaSet b = SymbolicFormulaSet { unSFS :: BSet.BoundSet b SymbolicFormula }
  deriving (Eq, Ord, Show, Generic, NFData, Hashable)

instance BSet.IsBound b => Solver.Lattice (SymbolicFormulaSet b) where
    bot   = SymbolicFormulaSet BSet.empty
    (SymbolicFormulaSet x) `merge` (SymbolicFormulaSet y) = SymbolicFormulaSet $ BSet.union x y

instance BSet.IsBound b => Solver.ExtLattice (SymbolicFormulaSet b) where
    top   = SymbolicFormulaSet BSet.Universe



--
-- * Arithemtic Value Analysis
--

data ArithmeticAnalysis = ArithmeticAnalysis
    deriving (Typeable)


--
-- * Arithemtic Value Variable Generator
--

type ArithResult = ValueTree.Tree SimpleFieldIndex (SymbolicFormulaSet BSet.Bound10)

arithmeticValue :: NodeAddress -> Solver.TypedVar ArithResult
arithmeticValue addr = case I.getNode addr of

    I.Node I.Literal [t, I.Node (I.StringValue v) _] | isIntType t -> case parseInt v of
        Just cint -> Solver.mkVariable (idGen addr) [] (compose $ BSet.singleton $ Ar.mkConst cint)
        Nothing   -> Solver.mkVariable (idGen addr) [] (compose $ BSet.singleton $ Ar.mkVar $ Constant (I.getNode addr) addr)

    I.Node I.Variable _ | Q.isLoopIterator addr -> Solver.mkVariable (idGen addr) [] (compose $ BSet.Universe)

    I.Node I.Variable (t:_) | isIntType t && isFreeVariable addr ->
        Solver.mkVariable (idGen addr) [] (compose $ BSet.singleton $ Ar.mkVar $ Variable (I.getNode addr) addr)

    I.Node I.CastExpr (t:_) | isIntType t -> var
      where
        var = Solver.mkVariable (idGen addr) [con] Solver.bot
        con = Solver.forward (arithmeticValue $ I.goDown 1 addr) var

    _ -> dataflowValue addr analysis ops

  where

    analysis = (mkDataFlowAnalysis ArithmeticAnalysis "A" arithmeticValue) {
        initialValueHandler = toInitialValue,
        initialValue = compose $ BSet.singleton $ Ar.zero
    }

    idGen = mkVarIdentifier analysis

    compose = ComposedValue.toComposed . SymbolicFormulaSet
    extract = unSFS . ComposedValue.toValue

    toInitialValue a = case Q.getNodeType a of
        I.Literal  -> value
        I.Variable -> value
        _          -> Solver.top
      where
        value = compose $ BSet.singleton $ Ar.mkVar $ Constant (I.getNode a) a

    ops = [ add, mul, sub, div, mod, ptrFromRef]

    add = OperatorHandler cov dep (val Ar.addFormula)
      where
        cov a = any (Q.isBuiltin a) [ "int_add", "uint_add", "gen_add" ]

    sub = OperatorHandler cov dep (val Ar.subFormula)
      where
        cov a = any (Q.isBuiltin a) [ "int_sub", "uint_sub", "gen_sub" ]

    mul = OperatorHandler cov dep (val Ar.mulFormula)
      where
        cov a = any (Q.isBuiltin a) [ "int_mul", "uint_mul", "gen_mul" ]

    div = OperatorHandler cov dep val
      where
        cov a = any (Q.isBuiltin a) [ "int_div", "uint_div", "gen_div" ]
        val _ a = compose $ tryDiv (extract $ Solver.get a lhs) (extract $ Solver.get a rhs)

        tryDiv x y = if not (BSet.isUniverse prod) && all (uncurry Ar.canDivide) (BSet.toList prod)
                        then BSet.map (uncurry Ar.divFormula) prod
                        else BSet.Universe
          where
            prod = BSet.cartProduct x y

    mod = OperatorHandler cov dep (val Ar.modFormula)
      where
        cov a = any (Q.isBuiltin a) [ "int_mod", "uint_mod", "gen_mod" ]

    ptrFromRef = OperatorHandler cov dep val
      where
        cov a = Q.isBuiltin a "ptr_from_ref"
        dep _ _ = []
        val _ _ = ComposedValue.composeElements [(component 1,compose res)]
          where
            res = BSet.singleton $ Ar.mkConst 0



    lhs = arithmeticValue $ I.goDown 2 addr
    rhs = arithmeticValue $ I.goDown 3 addr

    dep _ _ = Solver.toVar <$> [lhs, rhs]

    val op _ a = compose $ (BSet.lift2 op) (extract $ Solver.get a lhs) (extract $ Solver.get a rhs)



isIntType :: I.Tree -> Bool
isIntType (I.Node I.GenericType (I.Node (I.StringValue "int" ) _:_)) = True
isIntType (I.Node I.GenericType (I.Node (I.StringValue "uint") _:_)) = True
isIntType (I.Node I.TypeVariable _ )                                 = True
isIntType _ = False

isFreeVariable :: NodeAddress -> Bool
isFreeVariable v | (not . Q.isVariable) v = False
isFreeVariable v = isNothing (I.findDecl v)
