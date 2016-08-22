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

{-# LANGUAGE ForeignFunctionInterface #-}

module Insieme.Adapter where

import Control.Exception
import Control.Monad
import Data.Foldable
import Data.Maybe
import Data.Tree
import Debug.Trace
import Foreign
import Foreign.C.String
import Foreign.C.Types
import Foreign.Marshal.Array
import qualified Data.ByteString.Char8 as BS8
import qualified Insieme.Analysis.Alias as Alias
import qualified Insieme.Analysis.Arithmetic as Arith
import qualified Insieme.Analysis.Boolean as AnBoolean
import qualified Insieme.Analysis.Solver as Solver
import qualified Insieme.Inspire as IR
import qualified Insieme.Inspire.BinaryParser as BinPar
import qualified Insieme.Inspire.NodeAddress as Addr
import qualified Insieme.Inspire.Utils as IRUtils
import qualified Insieme.Utils.Arithmetic as Ar
import qualified Insieme.Utils.BoundSet as BSet

import qualified Insieme.Analysis.Entities.SymbolicFormula as SymbolicFormula
import qualified Insieme.Analysis.Framework.PropertySpace.ComposedValue as ComposedValue

--
-- * HSobject
--

foreign export ccall "hat_freeStablePtr"
    freeStablePtr :: StablePtr a -> IO ()

--
-- * Tree
--

-- | Get a stable C pointer to the Haskell Inspire representation of
-- the input (binary dump).
passIR :: CString -> CSize -> IO (StablePtr IR.Inspire)
passIR dump_c length_c = do
    dump <- BS8.packCStringLen (dump_c, fromIntegral length_c)
    let Right ir = BinPar.parseBinaryDump dump
    newStablePtr ir

foreign export ccall "hat_passIR"
    passIR :: CString -> CSize -> IO (StablePtr IR.Inspire)

-- | Calculate the size of the buffer which contains the Haskell
-- representation of the Inspire tree.
treeLength :: StablePtr IR.Inspire -> IO CSize
treeLength ir_c = fromIntegral . length . IR.getTree <$> deRefStablePtr ir_c

foreign export ccall "hat_IR_length"
    treeLength :: StablePtr IR.Inspire -> IO CSize

-- | Print default representation of the given tree.
printTree :: StablePtr IR.Inspire -> IO ()
printTree ir_c = deRefStablePtr ir_c >>= (print . IR.getTree)

foreign export ccall "hat_IR_printTree"
    printTree :: StablePtr IR.Inspire -> IO ()

-- | 2-dimensional drawing of the Inspire subtree located at the given
-- address.
printNode :: StablePtr Addr.NodeAddress -> IO ()
printNode addr_c = do
    addr <- deRefStablePtr addr_c
    putStrLn . drawTree $ show <$> Addr.getNode addr

foreign export ccall "hat_IR_printNode"
    printNode :: StablePtr Addr.NodeAddress -> IO ()

--
-- * Address
--

-- | Return a stable C pointer to a Haskell vector containing the
-- given NodeAddress.
passAddress :: StablePtr IR.Inspire -> Ptr CSize -> CSize
            -> IO (StablePtr Addr.NodeAddress)
passAddress ir_c path_c length_c = do
    ir   <- deRefStablePtr ir_c
    path <- peekArray (fromIntegral length_c) path_c
    newStablePtr $ Addr.mkNodeAddress (fromIntegral <$> path) ir

foreign export ccall "hat_passAddress"
    passAddress :: StablePtr IR.Inspire -> Ptr CSize -> CSize
                -> IO (StablePtr Addr.NodeAddress)

-- | Return the size of the buffer representing the Haskell NodeAddress.
addrLength :: StablePtr Addr.NodeAddress -> IO CSize
addrLength addr_c = do
    addr <- deRefStablePtr addr_c
    return . fromIntegral . length . Addr.getPath $ addr

foreign export ccall "hat_addr_length"
    addrLength :: StablePtr Addr.NodeAddress -> IO CSize

-- | Convert the address contained in the given buffer into a proper
-- C++ vector of type @vector<size_t>@.
addrToArray :: StablePtr Addr.NodeAddress -> Ptr CSize -> IO ()
addrToArray addr_c dst = do
    addr <- deRefStablePtr addr_c
    pokeArray dst $ fromIntegral <$> Addr.getPath addr

foreign export ccall "hat_addr_toArray"
    addrToArray :: StablePtr Addr.NodeAddress -> Ptr CSize -> IO ()

--
-- * Parse IR
--

parseIR :: String -> IO IR.Inspire
parseIR ircode = do
    ptr_ir <- BS8.useAsCStringLen (BS8.pack ircode) parseIR_c'
    ir     <- deRefStablePtr ptr_ir
    freeStablePtr ptr_ir
    return ir
  where
    parseIR_c' (s,l) = parseIR_c s (fromIntegral l)

foreign import ccall "hat_parseIR"
    parseIR_c :: CString -> CSize -> IO (StablePtr IR.Inspire)

--
-- * Analysis
--

findDecl :: StablePtr Addr.NodeAddress -> IO (StablePtr Addr.NodeAddress)
findDecl addr_c = do
    addr <- deRefStablePtr addr_c
    case IRUtils.findDecl addr of
        Nothing -> return $ castPtrToStablePtr nullPtr
        Just a  -> newStablePtr a

foreign export ccall "hat_findDecl"
    findDecl :: StablePtr Addr.NodeAddress -> IO (StablePtr Addr.NodeAddress)

checkBoolean :: StablePtr Addr.NodeAddress -> IO (CInt)
checkBoolean addr_c = handleAll (return . fromIntegral . fromEnum $ AnBoolean.Both) $ do
    addr <- deRefStablePtr addr_c
    evaluate . fromIntegral . fromEnum . ComposedValue.toValue .  Solver.resolve . AnBoolean.booleanValue $ addr

foreign export ccall "hat_checkBoolean"
    checkBoolean :: StablePtr Addr.NodeAddress -> IO (CInt)

arithValue :: StablePtr Addr.NodeAddress -> IO (Ptr CArithmeticSet)
arithValue addr_c = do
    addr <- deRefStablePtr addr_c
    let results = ComposedValue.toValue $ Solver.resolve (Arith.arithmeticValue addr)
    passFormulaSet $ BSet.map (fmap SymbolicFormula.getAddr) results

foreign export ccall "hat_arithmeticValue"
    arithValue :: StablePtr Addr.NodeAddress -> IO (Ptr CArithmeticSet)

checkAlias :: StablePtr Addr.NodeAddress -> StablePtr Addr.NodeAddress -> IO CInt
checkAlias x_c y_c = handleAll (return . fromIntegral . fromEnum $ Alias.MayAlias) $ do
    x <- deRefStablePtr x_c
    y <- deRefStablePtr y_c
    evaluate $ fromIntegral $ fromEnum $ Alias.checkAlias x y

foreign export ccall "hat_checkAlias"
    checkAlias :: StablePtr Addr.NodeAddress -> StablePtr Addr.NodeAddress -> IO CInt

--
-- * Arithemtic
--

type CArithmeticValue = ()
type CArithmeticFactor = ()
type CArithmeticProduct = ()
type CArithmeticTerm = ()
type CArithmeticFormula = ()
type CArithmeticSet = ()

foreign import ccall "hat_arithmetic_value"
    arithmeticValue :: Ptr CSize -> CSize -> IO (Ptr CArithmeticValue)

foreign import ccall "hat_arithemtic_factor"
    arithemticFactor :: Ptr CArithmeticValue -> CInt -> IO (Ptr CArithmeticFactor)

foreign import ccall "hat_arithmetic_product"
    arithmeticProduct :: Ptr (Ptr CArithmeticFactor) -> CSize -> IO (Ptr CArithmeticProduct)

foreign import ccall "hat_arithmetic_term"
    arithmeticTerm :: Ptr CArithmeticProduct -> CULong -> IO (Ptr CArithmeticTerm)

foreign import ccall "hat_arithmetic_formula"
    arithmeticFormula :: Ptr (Ptr CArithmeticTerm) -> CSize -> IO (Ptr CArithmeticFormula)

foreign import ccall "hat_arithmetic_set"
    arithmeticSet :: Ptr (Ptr CArithmeticFormula) -> CInt -> IO (Ptr CArithmeticSet)


passFormula :: Integral c => Ar.Formula c Addr.NodeAddress -> IO (Ptr CArithmeticFormula)
passFormula formula = do
    terms <- forM (Ar.terms formula) passTerm
    withArrayLen' terms arithmeticFormula
  where
    passTerm :: Integral c => Ar.Term c Addr.NodeAddress -> IO (Ptr CArithmeticTerm)
    passTerm term = do
        product <- passProduct (Ar.product term)
        arithmeticTerm product (fromIntegral $ Ar.coeff term)

    passProduct :: Integral c => Ar.Product c Addr.NodeAddress -> IO (Ptr CArithmeticProduct)
    passProduct product = do
        factors <- forM (Ar.factors product) passFactor
        withArrayLen' factors arithmeticProduct

    passFactor :: Integral c => Ar.Factor c Addr.NodeAddress -> IO (Ptr CArithmeticFactor)
    passFactor factor = do
        value <- passValue (Ar.base factor)
        arithemticFactor value (fromIntegral $ Ar.exponent factor)

    passValue :: Addr.NodeAddress -> IO (Ptr CArithmeticValue)
    passValue addr = withArrayLen' (fromIntegral <$> Addr.getPath addr) arithmeticValue

    withArrayLen' :: Storable a => [a] -> (Ptr a -> CSize -> IO b) -> IO b
    withArrayLen' xs f = withArrayLen xs (\s a -> f a (fromIntegral s))


passFormulaSet :: Integral c => BSet.BoundSet bb (Ar.Formula c Addr.NodeAddress)
               -> IO (Ptr CArithmeticSet)
passFormulaSet BSet.Universe = arithmeticSet nullPtr (-1)
passFormulaSet bs = do
    formulas <- mapM passFormula (BSet.toList bs)
    withArrayLen' formulas arithmeticSet
  where
    withArrayLen' :: Storable a => [a] -> (Ptr a -> CInt -> IO b) -> IO b
    withArrayLen' xs f = withArrayLen xs (\s a -> f a (fromIntegral s))


--
-- * Arithmetic Tests
--

testFormulaZero :: IO (Ptr CArithmeticFormula)
testFormulaZero = passFormula Ar.zero

foreign export ccall "hat_test_formulaZero"
    testFormulaZero :: IO (Ptr CArithmeticFormula)


testFormulaOne :: IO (Ptr CArithmeticFormula)
testFormulaOne = passFormula Ar.one

foreign export ccall "hat_test_formulaOne"
    testFormulaOne :: IO (Ptr CArithmeticFormula)


testFormulaExample1 :: StablePtr Addr.NodeAddress -> IO (Ptr CArithmeticFormula)
testFormulaExample1 addr_c = do
    addr <- deRefStablePtr addr_c
    passFormula $ Ar.Formula [Ar.Term 2 (Ar.Product [Ar.Factor addr 2])]

foreign export ccall "hat_test_formulaExample1"
    testFormulaExample1 :: StablePtr Addr.NodeAddress -> IO (Ptr CArithmeticFormula)


testFormulaExample2 :: StablePtr Addr.NodeAddress -> IO (Ptr CArithmeticFormula)
testFormulaExample2 addr_c = do
    addr <- deRefStablePtr addr_c
    passFormula $ Ar.Formula [ Ar.Term 1 (Ar.Product []), Ar.Term 2 (Ar.Product [Ar.Factor addr 2, Ar.Factor addr 4]) ]

foreign export ccall "hat_test_formulaExample2"
    testFormulaExample2 :: StablePtr Addr.NodeAddress -> IO (Ptr CArithmeticFormula)


--
-- * Utilities
--

handleAll :: IO a -> IO a -> IO a
handleAll dummy action = catch action $ \e -> do
    putStrLn $ "Exception: " ++ show (e :: SomeException)
    dummy
