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

module Insieme.Inspire.NodeAddress (
    -- * Node Address
    NodeAddress,
    prettyShow,

    -- ** Constructor
    mkNodeAddress,

    -- ** Queries
    getPathReversed,
    getNode,
    getNodeType,
    getParent,
    getPath,
    getAbsolutePath,
    depth,
    depthAbsolute,
    numChildren,
    getChildren,
    getIndex,
    isRoot,
    getRoot,
    getRootAddress,
    isChildOf,

    -- *** Semantic Queries
    isLoopIterator,
    hasEnclosingStatement,
    isEntryPoint,
    isEntryPointParameter,

    -- ** Navigation
    goRel,
    goUp,
    goUpX,
    goDown,
    goLeft,
    goRight,

    -- ** Address Clipping
    append,
    crop,
) where

import Control.DeepSeq
import Data.List (foldl',isSuffixOf)
import Data.Maybe
import Debug.Trace
import GHC.Generics (Generic)
import Insieme.Inspire.Query
import Insieme.Utils
import qualified Data.Hashable as Hash
import qualified Insieme.Inspire as IR

type NodePath = [Int]

data NodeAddress = NodeAddress { getPathReversed     :: NodePath,
                                 getNode             :: IR.Tree,
                                 getParent           :: Maybe NodeAddress,
                                 getRoot             :: IR.Tree,
                                 getAbsoluteRootPath :: NodePath
                               }
  deriving (Generic, NFData)

instance Eq NodeAddress where
    x == y = getNode x == getNode y
             && getPathReversed x == getPathReversed y
             && getRoot x == getRoot y

instance Ord NodeAddress where
    compare x y = r1 `thenCompare` r2 `thenCompare` r3
        where
            r1 = compare (getNode x) (getNode y)
            r2 = compare (getPathReversed x) (getPathReversed y)
            r3 = compare (getRoot x) (getRoot y)
            

instance Show NodeAddress where
    show = prettyShow

instance Hash.Hashable NodeAddress where
    hashWithSalt s n = Hash.hashWithSalt s $ getPathReversed n
    hash n = Hash.hash $ getPathReversed n

instance NodeReference NodeAddress where
    node  = getNode
    child = goDown

prettyShow :: NodeAddress -> String
prettyShow na = '0' : concat ['-' : show x | x <- getPath na]

-- | Create a 'NodeAddress' from a list of indizes and a root node.
mkNodeAddress :: [Int] -> IR.Tree -> NodeAddress
mkNodeAddress xs root = foldl' (flip goDown) (NodeAddress [] root Nothing root []) xs

-- | Slow, use 'getPathReversed' where possible
getPath :: NodeAddress -> NodePath
getPath = reverse . getPathReversed

getAbsolutePath :: NodeAddress -> NodePath
getAbsolutePath a =  reverse $ getPathReversed a ++ getAbsoluteRootPath a

depth :: NodeAddress -> Int
depth = length . getPathReversed

depthAbsolute :: NodeAddress -> Int
depthAbsolute a = length $ getPathReversed a ++ getAbsoluteRootPath a

-- | Get the number of children of a given node.
numChildren :: NodeAddress -> Int
numChildren = length . IR.getChildren . getNode

getChildren :: NodeAddress -> [NodeAddress]
getChildren a = (flip goDown) a <$> [0..numChildren a - 1]

-- | Returns the position, of the given node, in its parent's list of children.
getIndex :: NodeAddress -> Int
getIndex a | isRoot a = error "Can't obtain index of a root address!"
getIndex a = head $ getPathReversed a

isRoot :: NodeAddress -> Bool
isRoot = isNothing . getParent

getRootAddress :: NodeAddress -> NodeAddress
getRootAddress a = mkNodeAddress [] (getRoot a)

isChildOf :: NodeAddress -> NodeAddress -> Bool
a `isChildOf` b = getRoot a == getRoot b && (getPathReversed b `isSuffixOf` getPathReversed a)

-- | Return a node address relative to the given one; a negative integer means
-- going up so many levels; zero or a positive integer means going to the
-- respective child.
goRel :: [Int] -> NodeAddress -> NodeAddress
goRel []     = id
goRel (i:is) | i < 0     = goRel is . last . take (1-i) . iterate goUp
             | otherwise = goRel is . goDown i

goUp :: NodeAddress -> NodeAddress
goUp na | isRoot na = trace "No parent for Root" undefined
goUp na = fromJust (getParent na)

goUpX :: Int -> NodeAddress -> NodeAddress
goUpX 0 a = a
goUpX n a = goUpX (n-1) $ goUp a

goDown :: Int -> NodeAddress -> NodeAddress
goDown x parent@(NodeAddress xs n _ ir r) = NodeAddress (x : xs) n' (Just parent) ir r
  where
    n' = IR.getChildren n !! x

goLeft :: NodeAddress -> NodeAddress
goLeft na@(NodeAddress xs _ _             _ _) | head xs == 0 = na
goLeft na@(NodeAddress _  _ Nothing       _ _) = na
goLeft    (NodeAddress xs _ (Just parent) _ _) = goDown (head xs - 1) parent

goRight :: NodeAddress -> NodeAddress
goRight na@(NodeAddress _  _ Nothing       _ _) = na
goRight    (NodeAddress xs _ (Just parent) _ _) = goDown (head xs + 1) parent

append :: NodeAddress -> NodeAddress -> NodeAddress
append a b | isRoot a = b
append a b | isRoot b = a
append a b = NodeAddress {
                getPathReversed     = (getPathReversed b) ++ (getPathReversed a),
                getNode             = getNode b,
                getRoot             = getRoot a,
                getParent           = Just $ append a $ fromJust $ getParent b,
                getAbsoluteRootPath = getAbsoluteRootPath a
             }

-- | Creates a 'NodeAddress' relative from the give node.
crop :: NodeAddress -> NodeAddress
crop a = NodeAddress [] (getNode a) Nothing (getNode a) ((getPathReversed a) ++ (getAbsoluteRootPath a))

-- | Returns 'True' if given variable (in declaration) is a loop iterator.
isLoopIterator :: NodeAddress -> Bool
isLoopIterator a = (depth a >= 2) && ((==IR.ForStmt) $ getNodeType $ goUp $ goUp a)

hasEnclosingStatement :: NodeAddress -> Bool
hasEnclosingStatement a = case getNode a of
    IR.Node n _ | (IR.toNodeKind n) == IR.Statement -> True
    IR.Node n _ | n /= IR.LambdaExpr && n /= IR.Variable && (IR.toNodeKind n) == IR.Expression -> True
    _ -> not (isRoot a) && hasEnclosingStatement (fromJust $ getParent a)

isEntryPoint :: NodeAddress -> Bool
isEntryPoint a = isRoot a || not (hasEnclosingStatement $ fromJust $ getParent a)

isEntryPointParameter :: NodeAddress -> Bool
isEntryPointParameter v | (not . isVariable) v = False
isEntryPointParameter v | isRoot v = False
isEntryPointParameter v = case getNode $ fromJust $ getParent v of
            IR.Node IR.Parameters _ -> not $ hasEnclosingStatement v
            _                     -> False
