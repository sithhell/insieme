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

module Insieme.Analysis.DynamicCallSites where

import Control.DeepSeq
import qualified Data.AbstractSet as Set
import Data.Typeable
import Data.Hashable
import GHC.Generics (Generic)

import Insieme.Inspire (NodeAddress)
import Insieme.Analysis.Utils.CppSemantic
import qualified Insieme.Inspire as I
import qualified Insieme.Query as Q

import qualified Insieme.Solver as Solver

--
-- * CallSite Results
--

newtype CallSite = CallSite NodeAddress
 deriving (Eq, Ord, Generic, NFData, Hashable)

instance Show CallSite where
    show (CallSite na) = "Call@" ++ (I.prettyShow na)

--
-- * CallSite Lattice
--

type CallSiteSet = Set.Set CallSite

instance Solver.Lattice CallSiteSet where
    bot   = Set.empty
    merge = Set.union


--
-- * DynamicCalls Analysis
--

data DynamicCallsAnalysis = DynamicCallsAnalysis
    deriving (Typeable)

dynamicCallsAnalysis :: Solver.AnalysisIdentifier
dynamicCallsAnalysis = Solver.mkAnalysisIdentifier DynamicCallsAnalysis "DynCalls"


--
-- * DynamicCalls Variable Generator
--

dynamicCalls :: NodeAddress -> Solver.TypedVar CallSiteSet
dynamicCalls addr = case Q.getNodeType addr of

    -- for root elements => collect all dynamic call sites
    _ | I.isRoot addr -> var

    -- for all other nodes, redirect to the root node
    _ -> dynamicCalls $ I.getRootAddress addr

  where

    idGen = Solver.mkIdentifierFromExpression dynamicCallsAnalysis

    var = Solver.mkVariable (idGen addr) [con] Solver.bot
    con = Solver.constant allNonLazyOpCalls var

    allCalls = Set.fromList $ CallSite <$> I.collectAllPrune isDynamicBoundCall skipTypes (I.getRootAddress addr)
      where
        -- NOTE: this is a simplification, since types may contain functions with dynamically bound calls
        skipTypes node = if Q.isType node then I.PruneHere else I.NoPrune

    allNonLazyOpCalls = Set.filter f allCalls
      where
        f (CallSite a) = not inLazyOp
          where
            inLazyOp = case getEnclosingLambda a of
              Just f -> any (I.isBuiltin $ I.node f) ["bool_and","bool_or","ite"]
              Nothing -> False
