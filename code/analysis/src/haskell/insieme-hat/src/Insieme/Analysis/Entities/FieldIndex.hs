
module Insieme.Analysis.Entities.FieldIndex (
    
    -- a type class for field indices
    FieldIndex,
    join,
    project,
    field,
    index,
    component,
    
    -- an example implementation
    SimpleFieldIndex
    
) where

import Data.Int
import Data.Typeable
import Insieme.Utils.Arithmetic
import Insieme.Utils.ParseInt
import Insieme.Analysis.Entities.SymbolicFormula
 
import qualified Data.Set as Set

class (Eq v, Ord v, Show v, Typeable v) => FieldIndex v where
        {-# MINIMAL join, project, field, index #-}
        join :: [v] -> [v] -> Maybe [v]
        project :: [v] -> v -> [v]
        
        field :: String -> v
        index :: SymbolicFormula -> v

        component :: Int32 -> v
        component = index . mkConst . CInt32


-- A simple field index example --

data SimpleFieldIndex = 
              Field String
            | Index Int
            | UnknownIndex
    deriving(Eq,Ord,Typeable)
    
instance Show SimpleFieldIndex where
    show (Field s) = s
    show (Index i) = "[" ++ (show i) ++ "]"
    show UnknownIndex = "[*]"
    

-- | Merges the list of simple field indices of the two given lists    
simpleJoin :: [SimpleFieldIndex] -> [SimpleFieldIndex] -> Maybe [SimpleFieldIndex]
simpleJoin a b | (allFields a && allFields b) || (allIndices a && allIndices b) = 
    Just $ Set.toList . Set.fromList $ a ++ b
simpleJoin _ _ = Nothing

-- | Computes the list of indices to combine when accessing a specific indices
simpleProject :: [SimpleFieldIndex] -> SimpleFieldIndex -> [SimpleFieldIndex]
simpleProject is i = if elem i is then [i] else [UnknownIndex]
    
    
instance FieldIndex SimpleFieldIndex where
    join = simpleJoin
    project = simpleProject
    
    field n = Field n
    index a = case toConstant a of 
        Just i  -> Index (fromIntegral i)
        Nothing -> UnknownIndex 


isField :: SimpleFieldIndex -> Bool
isField (Field _) = True
isField _         = False

allFields :: [SimpleFieldIndex] -> Bool
allFields = all isField
        
allIndices :: [SimpleFieldIndex] -> Bool
allIndices = all $ not . isField
                
        
        