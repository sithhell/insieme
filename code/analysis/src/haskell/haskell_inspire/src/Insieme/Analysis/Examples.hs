{-# LANGUAGE PatternSynonyms #-}

module Insieme.Analysis.Examples where

import Data.Maybe
import Data.Tree
import Debug.Trace
import Insieme.Inspire.NodeAddress
import qualified Data.Sequence as Seq
import qualified Insieme.Inspire as IR

findDeclr :: NodeAddress -> Tree IR.Inspire -> NodeAddress
findDeclr start tree = findDeclr start tree
  where
    org = fromJust $ resolve start tree
    findDeclr addr tree

        | Just (Node IR.DeclarationStmt [ Node IR.Variable [_,v], _]) <- resolve addr tree
        , v == org
        = trace ("--> Declr found: " ++ show addr) addr

        | Just (Node IR.ForStmt _) <- resolve (goUp addr) tree
        , _ :>: x <- addr
        , x /= 0
        = trace ("checking for   : " ++ show addr) $ findDeclr (goDown 0 . goUp $ addr) tree

        | Just (Node IR.CompoundStmt _) <- resolve (goUp addr) tree
        , _ :>: x <- addr
        , x /= 0
        = trace ("going left from: " ++ show addr) $ findDeclr (goLeft addr) tree

        | Seq.null addr
        = error "no declr found"

        | Just _ <- resolve addr tree
        = trace ("going up   from: " ++ show addr) $ findDeclr (goUp addr) tree

        | otherwise
        = error "fix your shit"
