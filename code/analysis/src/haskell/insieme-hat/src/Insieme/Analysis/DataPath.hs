{-
 - Copyright (c) 2002-2016 Distributed and Parallel Systems Group,
 -                Institute of Computer Science,
 -               University of Innsbruck, Austria
 -
 - This file is part of the INSIEME Compiler and Runtime System.
 -
 - We provide the software of this file (below described as "INSIEME")
 - under GPL Version 3.0 on an AS IS basis, and do not warrant its
 - validity or performance.  We reserve the right to update, modify,
 - or discontinue this software at any time.  We shall have no
 - obligation to supply such updates or modifications or any other
 - form of support to you.
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
 -
 - All copyright notices must be kept intact.
 -
 - INSIEME depends on several third party software packages. Please
 - refer to http://www.dps.uibk.ac.at/insieme/license.html for details
 - regarding third party software licenses.
 -}

{-# LANGUAGE FlexibleInstances #-}

module Insieme.Analysis.DataPath where

import Data.Typeable

import Insieme.Inspire.NodeAddress
import qualified Data.Set as Set

import Insieme.Analysis.Entities.DataPath
import {-# SOURCE #-} Insieme.Analysis.Framework.Dataflow
import Insieme.Analysis.Framework.Utils.OperatorHandler

import qualified Insieme.Utils.BoundSet as BSet
import qualified Insieme.Utils.UnboundSet as USet

import Insieme.Analysis.Identifier
import Insieme.Analysis.Arithmetic

import qualified Insieme.Analysis.Solver as Solver

import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue
import qualified Insieme.Analysis.Framework.PropertySpace.ValueTree as ValueTree
import Insieme.Analysis.Entities.FieldIndex


--
-- * DataPath Lattice
--

type DataPathSet i = USet.UnboundSet (DataPath i)

instance (FieldIndex i) => Solver.Lattice (DataPathSet i) where
    bot   = USet.empty
    merge = USet.union

instance (FieldIndex i) => Solver.ExtLattice (DataPathSet i) where
    top = USet.Universe
    
    
--
-- * DataPath Analysis
--


data DataPathAnalysis = DataPathAnalysis
    deriving (Typeable)

dataPathAnalysis = Solver.mkAnalysisIdentifier DataPathAnalysis "DP"


--
-- * DataPath Variable Generator
--

dataPathValue :: (FieldIndex i) => NodeAddress -> Solver.TypedVar (ValueTree.Tree i (DataPathSet i))
dataPathValue addr = dataflowValue addr analysis ops
                
  where
  
    analysis = DataFlowAnalysis DataPathAnalysis dataPathAnalysis dataPathValue top
  
    idGen = mkVarIdentifier analysis
  
    top = compose USet.Universe
    
    compose = ComposedValue.toComposed
    
    -- add operator support
    
    ops = [ rootOp, member, element] -- TODO: add parent
    
    -- handle the data path root constructore --
    rootOp = OperatorHandler cov dep val
      where
        cov a = isBuiltin a "dp_root"
        
        dep a = []
         
        val a = compose $ USet.singleton root
    
    
    -- the handler for the member access path constructore --
    member = OperatorHandler cov dep val
      where
        cov a = isBuiltin a "dp_member"
        
        dep a = (Solver.toVar nestedPathVar) : (Solver.toVar fieldNameVar) : []
        
        val a = compose $ combine (paths a) fieldNames
            where
                combine = USet.lift2 $ \p i -> append p ((step . field) (toString i))
                fieldNames = ComposedValue.toValue $ Solver.get a fieldNameVar
                
        fieldNameVar = identifierValue $ goDown 3 addr
    
    
    -- the handler for the element and component access path constructore --
    element = OperatorHandler cov dep val
      where
        cov a = any (isBuiltin a) ["dp_element","dp_component"]
        
        dep a = (Solver.toVar nestedPathVar) : (Solver.toVar indexVar) : []
        
        val a = compose $ combine (paths a) indexes 
            where
                combine = USet.lift2 $ \p i -> append p ((step . index) i)
                indexes = BSet.toUnboundSet $ ComposedValue.toValue $ Solver.get a indexVar 
                
        indexVar = arithmeticValue $ goDown 3 addr
    
    
    -- common utilities --
    
    nestedPathVar = dataPathValue $ goDown 2 addr
    
    paths a = ComposedValue.toValue $ Solver.get a nestedPathVar