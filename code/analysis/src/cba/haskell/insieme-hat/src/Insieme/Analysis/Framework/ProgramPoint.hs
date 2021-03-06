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

module Insieme.Analysis.Framework.ProgramPoint where

import Data.Maybe

import qualified Data.AbstractSet as Set

import qualified Insieme.Inspire as IR
import qualified Insieme.Query as Q
import qualified Insieme.Utils.BoundSet as BSet

import Insieme.Analysis.Entities.ProgramPoint
import Insieme.Analysis.Predecessor
import Insieme.Analysis.Callable
import qualified Insieme.Solver as Solver
import Insieme.Analysis.Framework.Utils.OperatorHandler
import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue

--
-- A generic variable and constraint generator for variables describing
-- properties of program points (e.g. the state of a heap object after
-- the execution of a given expression)
--
programPointValue :: (Solver.ExtLattice a)
         => ProgramPoint                                    -- ^ the program point for which to compute a variable representing a state value
         -> (ProgramPoint -> Solver.Identifier)             -- ^ a variable ID generator function
         -> (ProgramPoint -> Solver.TypedVar a)             -- ^ a variable generator function for referenced variables
         -> [OperatorHandler a]                             -- ^ a list of operator handlers to intercept the interpretation of certain operators
         -> Solver.TypedVar a                               -- ^ the resulting variable representing the requested information

programPointValue pp@(ProgramPoint addr p) idGen analysis ops = case Q.getNodeType addr of

        -- allow operator handlers to intercept the interpretation of calls
        IR.CallExpr | p == Post -> ivar
            where

                extract = ComposedValue.toValue

                -- create a variable and an intertangled constraint
                ivar = Solver.mkVariable (idGen pp) [icon] Solver.bot
                icon = Solver.createConstraint idep ival ivar

                -- if an handler is active, use the handlers value, else the default
                idep a = (Solver.toVar targetVar) : (
                        if hasUniversalTarget a then [] else if isHandlerActive a then operatorDeps a else dep a
                    )
                ival a = if hasUniversalTarget a then Solver.top else if isHandlerActive a then operatorVal a else val a

                -- the variable storing the target of the call
                targetVar = callableValue (IR.goDown 1 addr)

                -- tests whether the set of callees is universal
                hasUniversalTarget a = BSet.isUniverse $ extract $ Solver.get a targetVar

                -- test whether any operator handlers are active
                getActiveHandlers a = if BSet.isUniverse callables then [] else concatMap f ops
                    where
                        callables = extract $ Solver.get a targetVar

                        targets = BSet.toList callables

                        f o = mapMaybe go targets
                            where
                                go l = if covers o trg then Just (o,trg) else Nothing
                                    where
                                        trg = toAddress l

                isHandlerActive a = not . null $ getActiveHandlers a

                -- compute the dependencies of the active handlers
                operatorDeps a = concat $ map go $ getActiveHandlers a
                    where
                        go (o,t) = dependsOn o t a

                -- compute the value computed by the active handlers
                operatorVal a = Solver.join $ map go $ getActiveHandlers a
                    where
                        go (o,t) = getValue o t a


        -- everything else is just forwarded
        _ -> var

    where

        -- by default, the state at each program point is the joined state of all its predecessors
        var = Solver.mkVariable (idGen pp) [con] Solver.bot
        con = Solver.createConstraint dep val var

        (dep,val) = mkPredecessorConstraintCredentials pp analysis


mkPredecessorConstraintCredentials :: (Solver.Lattice a)
        => ProgramPoint
        -> (ProgramPoint -> Solver.TypedVar a)
        -> (Solver.AssignmentView -> [Solver.Var],Solver.AssignmentView -> a)

mkPredecessorConstraintCredentials pp analysis = (dep,val)
    where
        predecessorVar = predecessor pp
        predecessorStateVars a = map analysis $ Set.toList $ unPL $ Solver.get a predecessorVar

        dep a = (Solver.toVar predecessorVar) : map Solver.toVar (predecessorStateVars a)
        val a = Solver.join $ map (Solver.get a) (predecessorStateVars a)
