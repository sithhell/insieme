{-# LANGUAGE FlexibleInstances #-}

module Insieme.Analysis.Predecessor (
    PredecessorList,
    predecessor
) where


import Insieme.Analysis.Entities.ProgramPoint
import Insieme.Inspire.NodeAddress
import qualified Insieme.Analysis.Solver as Solver

import Debug.Trace
import Data.Tree
import Data.Maybe
import Insieme.Inspire.NodeAddress
import Insieme.Analysis.Boolean
import Insieme.Analysis.Callable
import Insieme.Analysis.CallSite
import Insieme.Analysis.ExitPoint
import qualified Insieme.Inspire as IR
import qualified Insieme.Utils.UnboundSet as USet

import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue


--
-- * Predecessor Lattice
--

type PredecessorList = [ProgramPoint]

instance Solver.Lattice PredecessorList where
    join [] = []
    join xs = foldr1 (++) xs

--
-- * Predecessor Analysis
--


predecessor :: ProgramPoint -> Solver.TypedVar PredecessorList

-- Predecessor rules for pre program points:
predecessor p@(ProgramPoint a Pre) | isRoot a = var
    where 
        var = Solver.mkVariable (idGen p) [] []


predecessor p@(ProgramPoint a Pre) = case getNode parent of
    
    Node IR.CallExpr children -> single $
            if i == (length children) - 1 
            then ProgramPoint parent Pre                   -- start with last argument
            else ProgramPoint (goDown (i+1) parent) Post   -- eval arguments in reverse order
    
    Node IR.CompoundStmt stmts -> single $
            if i == 0                                      -- if it is the first statement
            then ProgramPoint parent Pre                   -- then go to the pre-state of the compound statement
            else ProgramPoint (goDown (i-1) parent) Post   -- else to the post state of the previous statement
    
    Node IR.Lambda   _ -> call_sites
    
    Node IR.BindExpr _ -> call_sites
    
    Node IR.Declaration _ -> single $ ProgramPoint parent Pre
    
    Node IR.DeclarationStmt _ -> single $ ProgramPoint parent Pre
    
    Node IR.ReturnStmt _ -> single $ ProgramPoint parent Pre
    
    Node IR.IfStmt _ | i == 0 -> single $ ProgramPoint parent Pre
    Node IR.IfStmt _ -> single $ ProgramPoint (goDown 0 parent) Post        -- todo: make dependent on result of conditional expression
    
    _ -> trace ( " Unhandled Pre Program Point: " ++ (show p) ++ " for parent " ++ (show $ getNode parent) ) $ error "unhandled case"
    
  where
  
    parent = fromJust $ getParent a
    i = getIndex a
  
    single :: ProgramPoint -> Solver.TypedVar PredecessorList
    single x = Solver.mkVariable (idGen p) [] [x] 
    
    call_sites = Solver.mkVariable (idGen p) [con] []
    con = Solver.createConstraint dep val call_sites
    
    dep a = [Solver.toVar callSitesVar]
    val a = foldr go [] $ Solver.get a callSitesVar
        where
            go = \(CallSite call) list -> (ProgramPoint (goDown 1 call) Post) : list
    
    callSitesVar = callSites parent
    
    
-- Predecessor rules for internal program points:    
predecessor  p@(ProgramPoint addr Internal) = case getNode addr of

    -- link to exit points of potential target functions
    Node IR.CallExpr _ -> var 
        where
        
            extract = ComposedValue.toValue
         
            var = Solver.mkVariable (idGen p) [con] []
            con = Solver.createConstraint dep val var
         
            dep a = Solver.toVar callableVar : map Solver.toVar (exitPointVars a)
            val a = (if callsLiteral then [litPredecessor] else []) ++ nonLiteralExit
                where
                    nonLiteralExit = foldr go [] (exitPointVars a)
                        where
                            go = \e l -> foldr (\(ExitPoint r) l -> (ProgramPoint r Post) : l) l (Solver.get a e)
                    
                    callsLiteral = any isLiteral (callableVal a)
                        where
                            isLiteral (Literal _) = True
                            isLiteral _ = False
                            
                    litPredecessor = ProgramPoint (goDown 1 addr) Post
                    
         
            callableVar = callableValue (goDown 1 addr)
            callableVal a = USet.toSet $ 
                    if USet.isUniverse callables 
                    then collectAllCallables addr 
                    else callables 
                where
                    callables = extract $ Solver.get a callableVar
            
            
            exitPointVars a = foldr (\t l -> exitPoints (toAddress t) : l) [] (callableVal a)
            
            
                
    _ -> trace ( " Unhandled Internal Program Point: " ++ (show p) ) $ error "unhandled case"



-- Predecessor rules for post program points:
predecessor p@(ProgramPoint a Post) = case getNode a of

    -- basic expressions are directly switching from Pre to Post
    Node IR.Variable         _ -> pre
    Node IR.Literal          _ -> pre
    Node IR.LambdaExpr       _ -> pre
    Node IR.LambdaReference  _ -> pre
    Node IR.BindExpr         _ -> pre
    
    -- call expressions are switching from Internal to Post
    Node IR.CallExpr _ -> single $ ProgramPoint a Internal
    
    -- declarationns are done once the init expression is done
    Node IR.Declaration _ -> single $ ProgramPoint (goDown 1 a) Post
    
    -- compound statements
    Node IR.CompoundStmt []    -> pre
    Node IR.CompoundStmt stmts -> single $ ProgramPoint (goDown ((length stmts) - 1) a) Post
    
    -- declaration statement
    Node IR.DeclarationStmt _ -> single $ ProgramPoint (goDown 0 a) Post
    
    -- conditional statement
    Node IR.IfStmt _ -> var
        where
            var = Solver.mkVariable (idGen p) [con] []
            con = Solver.createConstraint dep val var
 
            dep a = [Solver.toVar conditionValueVar]
            
            val a = case ComposedValue.toValue $ Solver.get a conditionValueVar of
                Neither     -> []
                AlwaysTrue  -> [thenBranch]
                AlwaysFalse -> [elseBranch]
                Both        -> [thenBranch,elseBranch]  
            
            conditionValueVar = booleanValue $ goDown 0 a
            
            thenBranch = ProgramPoint (goDown 1 a) Post
            elseBranch = ProgramPoint (goDown 2 a) Post 
    
    -- return statement
    Node IR.ReturnStmt _ -> single $ ProgramPoint (goDown 0 a) Post
    
    
    _ -> trace ( " Unhandled Post Program Point: " ++ (show p) ++ " for node " ++ (show $ getNode a) ) $ error "unhandled case"
    
  where
  
    single :: ProgramPoint -> Solver.TypedVar PredecessorList
    single x = Solver.mkVariable (idGen p) [] [x] 
    
    pre = single $ ProgramPoint a Pre


-- variable ID generator
idGen :: ProgramPoint -> Solver.Identifier
idGen p = Solver.mkIdentifier $ "pred_of:" ++ (show p)
