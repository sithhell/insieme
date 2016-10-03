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

{-# LANGUAGE BangPatterns #-}

module Insieme.Analysis.Solver (

    -- lattices
    Lattice,
    join,
    merge,
    bot,
    less,

    ExtLattice,
    top,

    -- analysis identifiers
    AnalysisIdentifier,
    mkAnalysisIdentifier,

    -- identifiers
    Identifier,
    mkIdentifierFromExpression,
    mkIdentifierFromProgramPoint,
    mkIdentifierFromMemoryStatePoint,
    mkIdentifierFromString,

    -- variables
    Var,
    TypedVar,
    mkVariable,
    toVar,
    getDependencies,
    getLimit,
    
    -- assignments
    Assignment,
    get,
    set,

    -- solver state
    SolverState,
    initState,
    assignment,
    numSteps,

    -- solver
    resolve,
    resolveS,
    resolveAll,
    solve,

    -- constraints
    createConstraint,
    createEqualityConstraint,
    forward,
    forwardIf,
    constant,

    -- debugging
    dumpSolverState,
    showSolverStatistic

) where

import Prelude hiding (lookup)
import Debug.Trace

import Control.DeepSeq
import Control.Exception
import Control.Monad (void)
import Data.Dynamic
import Data.Function
import Data.List hiding (insert,lookup)
import Data.Maybe
import Data.Tuple
import System.CPUTime
import System.Directory (doesFileExist)
import System.IO.Unsafe (unsafePerformIO)
import System.Process
import Text.Printf
import qualified Control.Monad.State.Strict as State
import qualified Data.ByteString.Char8 as BS
import qualified Data.Graph as Graph
import qualified Data.Hashable as Hash
import qualified Data.IntMap.Strict as IntMap
import qualified Data.Map.Strict as Map
import qualified Data.Set as Set

import Insieme.Utils
import Insieme.Inspire.NodeAddress
import Insieme.Analysis.Entities.Memory
import Insieme.Analysis.Entities.ProgramPoint



-- Lattice --------------------------------------------------

-- this is actually a bound join-semilattice
class (Eq v, Show v, Typeable v, NFData v) => Lattice v where
        {-# MINIMAL join | merge, bot #-}
        -- | combine elements
        join :: [v] -> v                    -- need to be provided by implementations
        join [] = bot                       -- a default implementation for the join operator
        join xs = foldr1 merge xs           -- a default implementation for the join operator
        -- | binary join
        merge :: v -> v -> v                -- a binary version of the join
        merge a b = join [ a , b ]          -- its default implementation derived from join
        -- | bottom element
        bot  :: v                           -- the bottom element of the join
        bot = join []                       -- default implementation
        -- | induced order
        less :: v -> v -> Bool              -- determines whether one element of the lattice is less than another
        less a b = (a `merge` b) == b       -- default implementation


class (Lattice v) => ExtLattice v where
        -- | top element
        top  :: v                           -- the top element of this lattice




-- Analysis Identifier -----

data AnalysisIdentifier = AnalysisIdentifier {
    aidToken :: TypeRep,
    aidName  :: String,
    aidHash  :: Int
}

instance Eq AnalysisIdentifier where
    (==) = (==) `on` aidToken

instance Ord AnalysisIdentifier where
    compare = compare `on` aidToken

instance Show AnalysisIdentifier where
    show = aidName


mkAnalysisIdentifier :: (Typeable a) => a -> String -> AnalysisIdentifier
mkAnalysisIdentifier a n = AnalysisIdentifier {
        aidToken = (typeOf a), aidName = n , aidHash = (Hash.hash n)
    }


-- Identifier -----


data IdentifierValue = 
          IDV_Expression NodeAddress
        | IDV_ProgramPoint ProgramPoint
        | IDV_MemoryStatePoint MemoryStatePoint
        | IDV_Other BS.ByteString  
    deriving (Eq,Ord,Show)


referencedAddress :: IdentifierValue -> Maybe NodeAddress
referencedAddress ( IDV_Expression   a )                                            = Just a
referencedAddress ( IDV_ProgramPoint (ProgramPoint a _) )                           = Just a
referencedAddress ( IDV_MemoryStatePoint (MemoryStatePoint (ProgramPoint a _) _ ) ) = Just a
referencedAddress ( IDV_Other _ )                                                   = Nothing



data Identifier = Identifier {
    analysis :: AnalysisIdentifier,
    idValue  :: IdentifierValue,
    idHash   :: Int
}


instance Eq Identifier where
    (==) (Identifier a1 v1 h1) (Identifier a2 v2 h2) =
            h1 == h2 && v1 == v2 && a1 == a2

instance Ord Identifier where
    compare (Identifier a1 v1 h1) (Identifier a2 v2 h2) =
            r0 `thenCompare` r1 `thenCompare` r2
        where
            r0 = compare h1 h2
            r1 = compare v1 v2
            r2 = compare a1 a2

instance Show Identifier where
        show (Identifier a v _) = (show a) ++ "/" ++ (show v)


mkIdentifierFromExpression :: AnalysisIdentifier -> NodeAddress -> Identifier
mkIdentifierFromExpression a n = Identifier {
        analysis = a,
        idValue = IDV_Expression n,
        idHash = Hash.hashWithSalt (aidHash a) n
    } 

mkIdentifierFromProgramPoint :: AnalysisIdentifier -> ProgramPoint -> Identifier
mkIdentifierFromProgramPoint a p = Identifier {
    analysis = a,
    idValue = IDV_ProgramPoint p,
    idHash = Hash.hashWithSalt (aidHash a) p
}

mkIdentifierFromMemoryStatePoint :: AnalysisIdentifier -> MemoryStatePoint -> Identifier
mkIdentifierFromMemoryStatePoint a m = Identifier {
    analysis = a,
    idValue = IDV_MemoryStatePoint m,
    idHash = Hash.hashWithSalt (aidHash a) m
}

mkIdentifierFromString :: AnalysisIdentifier -> String -> Identifier
mkIdentifierFromString a s = Identifier {
    analysis = a,
    idValue = IDV_Other $ BS.pack s,
    idHash = Hash.hashWithSalt (aidHash a) $ s
}

address :: Identifier -> Maybe NodeAddress
address = referencedAddress . idValue


-- Analysis Variables ---------------------------------------

-- general variables (management)
data Var = Var {
                index :: Identifier,                 -- the variable identifier
                constraints :: [Constraint],         -- the list of constraints
                bottom :: Dynamic,                   -- the bottom value for this variable
                valuePrint :: Assignment -> String   -- a utility for unpacking an printing a value assigned to this variable
        }

instance Eq Var where
        (==) a b = (index a) == (index b)

instance Ord Var where
        compare a b = compare (index a) (index b)

instance Show Var where
        show v = show (index v)


-- typed variables (user interaction)
newtype TypedVar a = TypedVar Var
        deriving ( Show, Eq, Ord )

mkVariable :: (Lattice a) => Identifier -> [Constraint] -> a -> TypedVar a
mkVariable i cs b = var
    where
        var = TypedVar ( Var i cs ( toDyn b ) print )
        print = (\a -> show $ get a var )

toVar :: TypedVar a -> Var
toVar (TypedVar x) = x


getDependencies :: Assignment -> TypedVar a -> [Var]
getDependencies a v = concat $ (go <$> (constraints . toVar) v)
    where
        go c = dependingOn c a 

getLimit :: (Lattice a) => Assignment -> TypedVar a -> a
getLimit a v = join (go <$> (constraints . toVar) v)
    where
        go c = get a' v
            where
                (a',_) = update c a 



-- Analysis Variable Maps -----------------------------------

newtype VarMap a = VarMap (IntMap.IntMap (Map.Map Var a))
    
emptyVarMap = VarMap IntMap.empty

lookup :: Var -> VarMap a -> Maybe a
lookup k (VarMap m) = (Map.lookup k) =<< (IntMap.lookup (idHash $ index k) m)

insert :: Var -> a -> VarMap a -> VarMap a
insert k v (VarMap m) = VarMap (IntMap.insertWith go (idHash $ index k) (Map.singleton k v) m)
    where
        go n o = Map.insert k v o

insertAll :: [(Var,a)] -> VarMap a -> VarMap a
insertAll [] m = m
insertAll ((k,v):xs) m = insertAll xs $ insert k v m 
                

keys :: VarMap a -> [Var]
keys (VarMap m) = foldr go [] m
    where 
        go im l = (Map.keys im) ++ l 


keysSet :: VarMap a -> Set.Set Var
keysSet (VarMap m) = foldr go Set.empty m
    where 
        go im s = Set.union (Map.keysSet im) s 


-- Assignments ----------------------------------------------

newtype Assignment = Assignment ( VarMap Dynamic )

instance Show Assignment where
    show a@( Assignment m ) = "Assignemnet {\n\t"
            ++
            ( intercalate ",\n\t\t" ( map (\v -> (show v) ++ " = " ++ (valuePrint v a) ) vars ) )
            ++
            "\n}"
        where
            vars = keys m


empty :: Assignment
empty = Assignment emptyVarMap

-- retrieves a value from the assignment
-- if the value is not present, the bottom value of the variable will be returned
get :: (Typeable a) => Assignment -> TypedVar a -> a
get (Assignment m) (TypedVar v) =
        fromJust $ (fromDynamic :: ((Typeable a) => Dynamic -> (Maybe a)) ) $ fromMaybe (bottom v) (lookup v m)


-- updates the value for the given variable stored within the given assignment
set :: (Typeable a, NFData a) => Assignment -> TypedVar a -> a -> Assignment
set (Assignment a) (TypedVar v) d = Assignment (insert v (toDyn d) a)


-- resets the values of the given variables within the given assignment
reset :: Assignment -> Set.Set IndexedVar -> Assignment
reset (Assignment m) vars = Assignment $ insertAll reseted m 
    where
        reseted = go <$> Set.toList vars
        go iv = (v,bottom v)
            where
                v = indexToVar iv

 

-- Constraints ---------------------------------------------

data Event =
          None                        -- ^ an update had no effect
        | Increment                   -- ^ an update was an incremental update
        | Reset                       -- ^ an update was not incremental



data Constraint = Constraint {
        dependingOn         :: Assignment -> [Var],
        update              :: (Assignment -> (Assignment,Event)),       -- update the assignment, a reset is allowed       
        updateWithoutReset  :: (Assignment -> (Assignment,Event)),       -- update the assignment, a reset is not allowed
        target              :: Var
   }





-- Variable Index -----------------------------------------------


-- a utility for indexing variables
data IndexedVar = IndexedVar Int Var

          
indexToVar :: IndexedVar -> Var
indexToVar (IndexedVar _ v) = v

toIndex :: IndexedVar -> Int
toIndex (IndexedVar i _ ) = i


instance Eq IndexedVar where
    (IndexedVar a _) == (IndexedVar b _) = a == b
    
instance Ord IndexedVar where
    compare (IndexedVar a _) (IndexedVar b _) = compare a b

instance Show IndexedVar where
    show (IndexedVar _ v) = show v
    


data VariableIndex = VariableIndex Int (VarMap IndexedVar)

emptyVarIndex :: VariableIndex
emptyVarIndex = VariableIndex 0 emptyVarMap

numVars :: VariableIndex -> Int
numVars (VariableIndex n _) = n

knownVariables :: VariableIndex -> Set.Set Var
knownVariables (VariableIndex _ m) = keysSet m

varToIndex :: VariableIndex -> Var -> (IndexedVar,VariableIndex)
varToIndex (VariableIndex n m) v = (res, VariableIndex nn nm)
    where
    
        ri = lookup v m
        
        nm = if isNothing ri then insert v ni m else m
        
        ni = IndexedVar n v           -- the new indexed variable, if necessary
        
        res = fromMaybe ni ri
        
        nn = if isNothing ri then n+1 else n


varsToIndex :: VariableIndex -> [Var] -> ([IndexedVar],VariableIndex)
varsToIndex i vs = foldr go ([],i) vs
    where
        go v (rs,i') = (r:rs,i'')
            where
                (r,i'') = varToIndex i' v 



-- Solver ---------------------------------------------------

-- a aggregation of the 'state' of a solver for incremental analysis

data SolverState = SolverState {
        assignment :: Assignment,
        variableIndex :: VariableIndex,        
        -- for performance evaluation
        numSteps  :: Map.Map AnalysisIdentifier Int,
        cpuTimes  :: Map.Map AnalysisIdentifier Integer,
        numResets :: Map.Map AnalysisIdentifier Int
    } 

initState = SolverState empty emptyVarIndex Map.empty Map.empty Map.empty



-- a utility to maintain dependencies between variables
data Dependencies = Dependencies (IntMap.IntMap (Set.Set IndexedVar))
        deriving Show

emptyDep = Dependencies IntMap.empty

addDep :: Dependencies -> IndexedVar -> [IndexedVar] -> Dependencies
addDep d _ [] = d
addDep d@(Dependencies m) t (v:vs) = addDep (Dependencies (IntMap.insertWith (\_ s -> Set.insert t s) (toIndex v) (Set.singleton t) m)) t vs

getDep :: Dependencies -> IndexedVar -> Set.Set IndexedVar
getDep (Dependencies d) v = fromMaybe Set.empty $ IntMap.lookup (toIndex v) d

getAllDep :: Dependencies -> IndexedVar -> Set.Set IndexedVar
getAllDep d i = collect d [i] Set.empty
    where
        collect d [] s = s
        collect d (v:_:_) s | v == i = s        -- we can stop if we reach the seed variable again
        collect d (v:vs) s = collect d ((Set.toList $ Set.difference dep s) ++ vs) (Set.union dep s)
            where
                dep = getDep d v 


-- solve for a single value
resolve :: (Lattice a) => SolverState -> TypedVar a -> (a,SolverState)
resolve i tv = (r,s)
    where
        (l,s) = resolveAll i [tv]
        r = head l

-- solve for a single value in state monad
resolveS :: (Lattice a) => TypedVar a -> State.State SolverState a
resolveS tv = do
    state <- State.get
    let (r, state') = resolve state tv
    State.put state'
    return r

-- solve for a list of variables
resolveAll :: (Lattice a) => SolverState -> [TypedVar a] -> ([a],SolverState)
resolveAll i tvs = (res <$> tvs,s)
    where
        s = solve i (toVar <$> tvs)
        ass = assignment s 
        res = get ass


-- solve for a set of variables
solve :: SolverState -> [Var] -> SolverState
solve init vs = solveStep (init {variableIndex = nindex}) emptyDep ivs
    where
        (ivs,nindex) = varsToIndex (variableIndex init) vs  


-- solve for a set of variables with an initial assignment
-- (the variable list is the work list)
-- Parameters:
--        the current state (assignment and known variables)
--        dependencies between variables
--        work list
solveStep :: SolverState -> Dependencies -> [IndexedVar] -> SolverState


-- solveStep _ _ (q:qs) | trace ("WS-length: " ++ (show $ (length qs) + 1) ++ " next: " ++ (show q)) $ False = undefined

-- empty work list
-- solveStep s _ [] | trace (dumpToJsonFile s "ass_meta") $ False = undefined                                           -- debugging assignment as meta-info for JSON dump
-- solveStep s _ [] | trace (dumpSolverState False s "graph") False = undefined                                         -- debugging assignment as a graph plot
-- solveStep s _ [] | trace (dumpSolverState True s "graph") $ trace (dumpToJsonFile s "ass_meta") $ False = undefined  -- debugging both
-- solveStep s _ [] | trace (showSolverStatistic s) $ False = undefined                                                 -- debugging performance data
solveStep s _ [] = s                                                                                                    -- work list is empty

-- compute next element in work list
solveStep (SolverState a i u t r) d (v:vs) = solveStep (SolverState resAss resIndex nu nt nr) resDep ds
        where
                -- profiling --
                ((resAss,resIndex,resDep,ds,numResets),dt) = measure go ()
                nt = Map.insertWith (+) aid dt t
                nu = Map.insertWith (+) aid  1 u
                nr = if numResets > 0 then Map.insertWith (+) aid numResets r else r
                aid = analysis $ index $ indexToVar v
                
                -- each constraint extends the result, the dependencies, and the vars to update
                go _ = foldr processConstraint (a,i,d,vs,0) ( constraints $ indexToVar v )  -- update all constraints of current variable
                processConstraint c (a,i,d,dv,numResets) = case ( update c a ) of
                        
                        (a',None)         -> (a',ni,nd,nv,numResets)                -- nothing changed, we are fine
                        
                        (a',Increment)    -> (a',ni,nd, uv ++ nv, numResets)        -- add depending variables to work list
                        
                        (a',Reset)        -> (ra,ni,nd, uv ++ nv, numResets+1)      -- handling a local reset
                            where 
                                dep = getAllDep nd trg
                                ra = if not $ Set.member trg dep                    -- if variable is not indirectly depending on itself  
                                    then reset a' dep                               -- reset all depending variables to their bottom value 
                                    else fst $ updateWithoutReset c a               -- otherwise insist on merging reseted value with current state
                                        
                    where
                            trg = v
                            dep = dependingOn c a
                            (idep,ni) = varsToIndex i dep
                            
                            newVarsList = filter f idep
                                where 
                                    f iv = toIndex iv >= numVars i
                                    
                            nd = addDep d trg idep
                            nv = newVarsList ++ dv
                            
                            uv = Set.elems $ getDep nd trg



-- Utils ---------------------------------------------------


-- | A simple constraint factory, taking as arguments
--   * a function to return the dependent variables of this constraint,
--   * the current value of the constraint,
--   * and the target variable for this constraint.
createConstraint :: (Lattice a) => ( Assignment -> [Var] ) -> ( Assignment -> a ) -> TypedVar a -> Constraint
createConstraint dep limit trg@(TypedVar var) = Constraint dep update update var
    where
        update a = case () of
                _ | value `less` current -> (                                a,      None)    -- nothing changed                                    
                _                        -> (set a trg (value `merge` current), Increment)    -- an incremental change                              
            where 
                value = limit a                                                               -- the value from the constraint      
                current = get a trg                                                           -- the current value in the assignment

                
-- creates a constraint of the form f(A) = A[b] enforcing equality
createEqualityConstraint dep limit trg@(TypedVar var) = Constraint dep update forceUpdate var
    where
        update a = case () of
                _ | value `less` current -> (              a,      None)    -- nothing changed                                    
                _ | current `less` value -> (set a trg value, Increment)    -- an incremental change                              
                _                        -> (set a trg value,     Reset)    -- a reseting change, heading in a different direction
            where 
                value = limit a                                             -- the value from the constraint      
                current = get a trg                                         -- the current value in the assignment

        forceUpdate a = case () of
                _ | value `less` current -> (                                a,      None)    -- nothing changed                                    
                _                        -> (set a trg (value `merge` current), Increment)    -- an incremental change                              
            where 
                value = limit a                                                               -- the value from the constraint      
                current = get a trg                                                           -- the current value in the assignment



-- creates a constraint of the form   A[a] \in A[b]
forward :: (Lattice a) => TypedVar a -> TypedVar a -> Constraint
forward a@(TypedVar v) b = createConstraint (\_ -> [v]) (\a' -> get a' a) b


-- creates a constraint of the form  x \sub A[a] => A[b] \in A[c]
forwardIf :: (Lattice a, Lattice b) => a -> TypedVar a -> TypedVar b -> TypedVar b -> Constraint
forwardIf a b@(TypedVar v1) c@(TypedVar v2) d = createConstraint dep upt d
    where
        dep = (\a' -> if less a $ get a' b then [v1,v2] else [v1] )
        upt = (\a' -> if less a $ get a' b then get a' c else bot )


-- creates a constraint of the form  x \in A[b]
constant :: (Lattice a) => a -> TypedVar a -> Constraint
constant x b = createConstraint (\_ -> []) (\a -> x) b



--------------------------------------------------------------

-- Profiling --

measure :: (a -> b) -> a -> (b,Integer)
measure f p = unsafePerformIO $ do    
        t1 <- getCPUTime
        r  <- evaluate $! f p
        t2 <- getCPUTime 
        return (r,t2-t1)


-- Debugging --

-- prints the current assignment as a graph
toDotGraph :: SolverState -> String
toDotGraph (SolverState a@( Assignment m ) varIndex _ _ _) = "digraph G {\n\t"
        ++
        "\n\tv0 [label=\"unresolved variable!\", color=red];\n"
        ++
        -- define nodes
        ( intercalate "\n\t" ( map (\v -> "v" ++ (show $ fst v ) ++ " [label=\"" ++ (show $ snd v) ++ " = " ++ (valuePrint (snd v) a) ++ "\"];" ) vars ) )
        ++
        "\n\t"
        ++
        -- define edges
        ( intercalate "\n\t" ( map (\d -> "v" ++ (show $ fst d) ++ " -> v" ++ (show $ snd d) ++ ";" ) deps ) )
        ++
        "\n}"
    where
    
        -- get set of known variables
        varSet = knownVariables varIndex

        -- a function collecting all variables a variable is depending on
        dep v = foldr (\c l -> (dependingOn c a) ++ l) [] (constraints v)

        -- list of all variables in the analysis
        allVars = Set.toList $ varSet

        -- the keys (=variables) associated with an index
        vars = Prelude.zip [1..] allVars

        -- a reverse lookup map for vars
        rev = Map.fromList $ map swap vars

        -- a lookup function for rev
        index v = fromMaybe 0 $ Map.lookup v rev

        -- computes the list of dependencies
        deps = foldr go [] vars
            where
                go = (\v l -> (foldr (\s l -> (fst v, index s) : l ) [] (dep $ snd v )) ++ l)


-- prints the current assignment to the file graph.dot and renders a pdf (for debugging)
dumpSolverState :: Bool -> SolverState -> FilePath -> String
dumpSolverState overwrite s f = unsafePerformIO $ do
  base <- solverToDot overwrite s f
  pdfFromDot base
  return ("Dumped assignment to " ++ base)

-- | Dump solver state to the given file name, using the dot format.
solverToDot :: Bool -> SolverState -> FilePath -> IO FilePath
solverToDot overwrite s base = target >>=
  \f -> writeFile (f ++ ".dot") (toDotGraph s) >> return f
  where
    target = if not overwrite then nonexistFile base "dot" else return base

-- | Generate a PDF from the dot file with the given basename.
pdfFromDot :: FilePath -> IO ()
pdfFromDot b = void (system $ "dot -Tpdf " ++ b ++ ".dot -o " ++ b ++ ".pdf")

-- | Generate a file name which does not exist yet. The arguments to
-- this function is the file name base, and the file extension. The
-- returned value is the base name only.
nonexistFile :: String -> String -> IO String
nonexistFile base ext = tryFile names
    where
      tryFile (f:fs) = doesFileExist (f ++ "." ++ ext)
                       >>= \e -> if e then tryFile fs else return f
      names  = base : [ iter n | n <- [ 1.. ]]
      iter n = concat [base, "-", show n]

toJsonMetaFile :: SolverState -> String
toJsonMetaFile (SolverState a@( Assignment m ) varIndex _ _ _) = "{\n"
        ++
        "    \"bodies\": {\n"
        ++
        ( intercalate ",\n" ( map print $ Map.toList store ) )
        ++
        "\n    }\n}"
    where

        addr = address . index
        
        vars = knownVariables varIndex

        store = foldr go Map.empty vars
            where
                go v m = if isJust $ addr v then m else Map.insert k (msg : Map.findWithDefault [] k m) m
                    where
                        k = fromJust $ addr v
                        i = index v
                        msg = (show . analysis $ i) ++ " = " ++ (valuePrint v a)

        print (a,ms) = "      \"" ++ (show a) ++ "\" : \"" ++ ( intercalate "<br>" ms) ++ "\""



dumpToJsonFile :: SolverState -> String -> String
dumpToJsonFile s file = unsafePerformIO $ do
         writeFile (file ++ ".json") $ toJsonMetaFile s
         return ("Dumped assignment into file " ++ file ++ ".json!")


showSolverStatistic :: SolverState -> String
showSolverStatistic s = 
        "===================================================== Solver Statistic ==============================================================================================\n" ++
        "         Analysis                #Vars              Updates          Updates/Var            ~Time[us]        ~Time/Var[us]               Resets           Resets/Var" ++
        "\n=====================================================================================================================================================================\n" ++
        ( intercalate "\n" (map print $ Map.toList grouped)) ++
        "\n---------------------------------------------------------------------------------------------------------------------------------------------------------------------\n" ++
        "           Total: " ++ (printf "%20d" numVars) ++ 
                        (printf " %20d" totalUpdates) ++ (printf " %20.3f" avgUpdates) ++ 
                        (printf " %20d" totalTime) ++ (printf " %20.3f" avgTime) ++
                        (printf " %20d" totalResets) ++ (printf " %20.3f" avgResets) ++
        "\n=====================================================================================================================================================================\n" ++
        analyseVarDependencies s ++
        "\n=====================================================================================================================================================================\n" 
    where
        vars = knownVariables $ variableIndex s
        
        grouped = foldr go Map.empty vars
            where
                go v m = Map.insertWith (+) ( analysis . index $ v ) (1::Int) m
        
        print (a,c) = printf " %16s %20d %20d %20.3f %20d %20.3f %20d %20.3f" name c totalUpdates avgUpdates totalTime avgTime totalResets avgResets
            where
                name = ((show a) ++ ":")
            
                totalUpdates = Map.findWithDefault 0 a $ numSteps s
                avgUpdates = ((fromIntegral totalUpdates) / (fromIntegral c)) :: Float
            
                totalTime = toMs $ Map.findWithDefault 0 a $ cpuTimes s
                avgTime = ((fromIntegral totalTime) / (fromIntegral c)) :: Float

                totalResets = Map.findWithDefault 0 a $ numResets s
                avgResets = ((fromIntegral totalResets) / (fromIntegral c)) :: Float


        numVars = Set.size vars
        
        totalUpdates = foldr (+) 0 (numSteps s)
        avgUpdates = ((fromIntegral totalUpdates) / (fromIntegral numVars)) :: Float
        
        totalTime = toMs $ foldr (+) 0 (cpuTimes s)
        avgTime = ((fromIntegral totalTime) / (fromIntegral numVars)) :: Float

        totalResets = foldr (+) 0 (numResets s)
        avgResets = ((fromIntegral totalResets) / (fromIntegral numVars)) :: Float
        
        toMs a = a `div` 1000000
        
        
analyseVarDependencies :: SolverState -> String
analyseVarDependencies s = 

            "  Number of SCCs: " ++ show (length sccs)   ++ "\n" ++
            "  largest SCCs:   " ++ show (take 10 $ reverse $ sort $ length . Graph.flattenSCC <$> sccs)

    where
    
        ass = assignment s 
    
        index = variableIndex s
        
        vars = knownVariables index
        
        -- getDep :: Dependencies -> IndexedVar -> Set.Set IndexedVar
        
        -- varToIndex :: VariableIndex -> Var -> (IndexedVar,VariableIndex)
        
        nodes = go <$> Set.toList vars
            where
                go v = (v,v,dep v)
                dep v = foldr (\c l -> (dependingOn c ass) ++ l) [] (constraints v)

        
        sccs = Graph.stronglyConnComp nodes
        
        
