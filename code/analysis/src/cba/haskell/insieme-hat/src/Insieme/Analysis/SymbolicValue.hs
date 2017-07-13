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
{-# LANGUAGE FlexibleInstances #-}

module Insieme.Analysis.SymbolicValue (
    SymbolicValue,
    SymbolicValueSet,
    symbolicValue,
) where

import Data.Maybe
import Data.Typeable
import Foreign
import Foreign.C.String
import Foreign.C.Types
import Insieme.Adapter (CRepPtr,CRepArr,CSetPtr,dumpIrTree,passBoundSet,updateContext)
import Insieme.Analysis.Entities.FieldIndex
import Insieme.Analysis.Entities.SymbolicFormula
import Insieme.Analysis.Framework.Utils.OperatorHandler
import Insieme.Inspire.BinaryDumper (dumpBinaryDump)
import Insieme.Inspire.Query
import Insieme.Inspire.Visit (findDecl)
import Insieme.Utils.ParseInt

import qualified Data.ByteString as BS
import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue
import qualified Insieme.Analysis.Framework.PropertySpace.ValueTree as ValueTree
import qualified Insieme.Analysis.Solver as Solver
import qualified Insieme.Context as Ctx
import qualified Insieme.Inspire as IR
import qualified Insieme.Inspire.Builder as Builder
import qualified Insieme.Inspire.NodeAddress as Addr
import qualified Insieme.Utils.Arithmetic as Ar
import qualified Insieme.Utils.BoundSet as BSet
import qualified Insieme.Utils.ParseIR as Lang

import Insieme.Analysis.Framework.Dataflow


--
-- * Symbolic Value Lattice
--

type SymbolicValue = IR.Tree

type SymbolicValueSet = BSet.BoundSet BSet.Bound10 SymbolicValue

instance Solver.Lattice SymbolicValueSet where
    bot   = BSet.empty
    merge = BSet.union

instance Solver.ExtLattice SymbolicValueSet where
    top   = BSet.Universe



--
-- * Symbolic Value Analysis
--

data SymbolicValueAnalysis = SymbolicValueAnalysis
    deriving (Typeable)


--
-- * Symbolic Value Variable and Constraint Generator
--

symbolicValue :: Addr.NodeAddress -> Solver.TypedVar (ValueTree.Tree SimpleFieldIndex SymbolicValueSet)
symbolicValue addr = case getNodeType addr of

    IR.Literal  -> Solver.mkVariable varId [] (compose $ BSet.singleton $ Addr.getNode addr)

    IR.Declaration -> var
      where
        var = Solver.mkVariable varId [con] Solver.bot
        con = Solver.createConstraint dep val var
    
        initValueVar = symbolicValue $ Addr.goDown 1 addr
        initValueVal a = extract $ Solver.get a initValueVar
    
        dep _ = [Solver.toVar initValueVar]
        val a = compose $ BSet.map toDecl $ initValueVal a
          where
            toDecl initVal = IR.mkNode IR.Declaration [declType,initVal] []
            
            declType = IR.goDown 0 $ Addr.getNode addr
    

    _ -> dataflowValue addr analysis ops

  where

    analysis = (mkDataFlowAnalysis SymbolicValueAnalysis "S" symbolicValue) {
        freeVariableHandler = freeVariableHandler,
        initialValueHandler = initialMemoryValue
    }

    varId = mkVarIdentifier analysis addr
    
    ops = [ operatorHandler ]
    
    -- a list of symbolic values of the arguments
    argVars = symbolicValue <$> ( tail . tail $ Addr.getChildren addr )
    
    -- the one operator handler that covers all operators
    operatorHandler = OperatorHandler cov dep val
      where
        
        -- TODO: apply on all builtins, also deriveds
        cov a = (getNodeType a == IR.Literal) && (not covered)
          where
            covered = any (isBuiltin a) [ "ref_deref", "ref_assign" ]
      
        -- if triggered, we will need the symbolic values of all arguments
        dep _ _ = Solver.toVar <$> argVars
      
        
        -- to get the new value, we have to take the cross product of all arguments
        val o a = compose $ BSet.map toCall argCombinations
          
          where
          
            argVals = extract . (Solver.get a) <$> argVars
            
            argCombinations = BSet.cartProductL argVals
            
            toCall args = IR.mkNode IR.CallExpr (resType : trg : args) []
            
            trg = Addr.getNode o
            
            resType = IR.goDown 0 $ Addr.getNode addr


    -- the handler determinign the value of a free variable

    freeVariableHandler a = var
      where
        var = Solver.mkVariable varId [] val
        val = compose $ BSet.singleton $ Addr.getNode a

            
    -- the handler assigning values to pre-existing memory locations (e.g. globals)
    
    initialMemoryValue a = 
        
        if isReference lit then compose $ BSet.singleton value else Solver.top
        
      where

        lit = Addr.getNode a
      
        value = Builder.deref lit
        


    -- utilities

    extract = ComposedValue.toValue
    compose = ComposedValue.toComposed








--
-- * FFI
--

foreign export ccall "hat_hs_symbolic_values"
  hsSymbolicValues :: StablePtr Ctx.Context -> StablePtr Addr.NodeAddress -> IO (CSetPtr SymbolicValue)
  

foreign import ccall "hat_c_mk_symbolic_value_set"
  mkCSymbolicValueSet :: CRepArr SymbolicValue -> CLLong -> IO (CSetPtr SymbolicValue)

  
hsSymbolicValues ctx_hs stmt_hs = do
    ctx  <- deRefStablePtr ctx_hs
    stmt <- deRefStablePtr stmt_hs
    let (res,ns) = Solver.resolve (Ctx.getSolverState ctx) (symbolicValue stmt)
    let ctx_c =  Ctx.getCContext ctx
    ctx_nhs <- newStablePtr $ ctx { Ctx.getSolverState = ns }
    updateContext ctx_c ctx_nhs
    passSymbolicValueSet ctx_c $ ComposedValue.toValue res
    
passSymbolicValueSet ctx s = do
    passBoundSet passSymbolicValue mkCSymbolicValueSet s
  where

    passSymbolicValue :: SymbolicValue -> IO (CRepPtr SymbolicValue)
    passSymbolicValue s = do
        dumpIrTree ctx s

    