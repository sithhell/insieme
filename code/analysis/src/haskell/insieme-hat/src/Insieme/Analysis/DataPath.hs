{-# LANGUAGE FlexibleInstances #-}

module Insieme.Analysis.DataPath where

import Insieme.Analysis.Solver
import Insieme.Inspire.NodeAddress
import qualified Data.Set as Set

import Insieme.Analysis.Framework.Dataflow

import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue
import qualified Insieme.Analysis.Framework.PropertySpace.ValueTree as ValueTree
import Insieme.Analysis.Framework.PropertySpace.FieldIndex


--
-- * DataPaths
--

data DataPath =
      Root
    | Field DataPath String
    | Element DataPath Int 
  deriving (Eq,Ord)
  

instance Show DataPath where
    show Root = "⊥"
    show (Field d f) = (show d) ++ "." ++ f
    show (Element d i) = (show d) ++ "." ++ (show i)
    
-- concatenation of paths
concatPath :: DataPath -> DataPath -> DataPath
concatPath a         Root  =                          a
concatPath a (  Field b s) =   Field (concatPath a b) s
concatPath a (Element b i) = Element (concatPath a b) i 



--
-- * DataPath Lattice
--

type DataPathSet = Set.Set DataPath

instance Lattice DataPathSet where
    join [] = Set.empty
    join xs = foldr1 Set.union xs
    
    
--
-- * DataPath Analysis
--

dataPathValue :: NodeAddress -> TypedVar (ValueTree.Tree SimpleFieldIndex DataPathSet)
dataPathValue addr = 
    case () of _
                | isBuiltin addr "dp_root"  -> mkVariable (idGen addr) [] (compose $ Set.singleton Root)
                | otherwise                 -> dataflowValue addr analysis []
                
  where
  
    analysis = DataFlowAnalysis "DP" dataPathValue top
  
    idGen = mkVarIdentifier analysis
  
    top = compose Set.empty     -- TODO: compute actual top
    
    compose = ComposedValue.toComposed
  