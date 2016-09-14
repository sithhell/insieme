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

module Insieme.Utils.Arithmetic where

import Control.Applicative
import Data.List hiding (product)
import Data.Maybe
import Data.Ord

import Prelude hiding (exponent, product)

--
-- * Formula
--

-- | Something like @-2 * x^2 * y^3 + 5 * z^1@ where 'c' denotes the type
-- of coefficients and exponents, and 'v' the type of variables.
data Formula c v = Formula { terms :: [Term c v] }
  deriving (Eq, Ord)

instance Functor (Formula c) where
    fmap f = Formula . map (fmap f) . terms

instance (Integral c, Show c, Show v) => Show (Formula c v) where
    show = prettyShow

convert :: (Integral c, Integral c', Ord v) => Formula c v -> Formula c' v
convert = Formula . map convertTerm . terms

normalize :: (Integral c, Ord v) => Formula c v -> Formula c v
normalize = Formula . filter ((/=0) . coeff)
                    . map merge
                    . groupBy (\x y -> product x == product y)
                    . sortOn product
                    . map normalizeTerm
                    . terms
  where
    merge ts = Term (sum $ coeff <$> ts) (product . head $ ts)

prettyShow :: (Integral c, Show c, Show v) => Formula c v -> String
prettyShow (Formula ts) = unempty . intercalate " + " $ prettyShowTerm <$> ts
  where
    unempty "" = "0"
    unempty xs = xs

isZero :: Formula c v -> Bool
isZero = null . terms

isConst :: Formula c v -> Bool
isConst = all termIsConst . terms

toConstant :: (Integral c) => Formula c v -> Maybe c
toConstant (Formula [])                    = Just 0
toConstant (Formula [Term c (Product [])]) = Just c
toConstant _                               = Nothing

zero :: (Integral c, Ord v) => Formula c v
zero = Formula []

one :: (Integral c, Ord v) => Formula c v
one = Formula [Term 1 (Product [])]

mkConst :: (Integral c, Ord v) => c -> Formula c v
mkConst 0 = zero
mkConst c = Formula [Term c (Product [])]

mkVar :: (Integral c, Ord v) => v -> Formula c v
mkVar v = Formula [Term 1 (Product [Factor v 1])]


--
-- * Parts of a Formula
--

-- | Something like @x^2@
data Factor c v = Factor { base     :: v,
                           exponent :: c }
  deriving (Eq, Ord, Show)

instance Functor (Factor c) where
    fmap f (Factor b e) = Factor (f b) e

compareFactor :: (Ord v) => Factor c v -> Factor c v-> Ordering
compareFactor = comparing base

convertFactor :: (Integral c, Integral c', Ord v) => Factor c v -> Factor c' v
convertFactor (Factor b e) = Factor b (fromIntegral e)

prettyShowFactor :: (Show c, Show v) => Factor c v -> String
prettyShowFactor (Factor b e) = show b ++ "^" ++ show e

-- | Something like @x^2 * y^3@
data Product c v = Product { factors :: [Factor c v] }
  deriving (Eq, Ord, Show)

instance Functor (Product c) where
    fmap f = Product . map (fmap f) . factors

convertProduct :: (Integral c, Integral c', Ord v)
               => Product c v -> Product c' v
convertProduct = Product . map convertFactor . factors

normalizeProduct :: (Integral c, Ord v) => Product c v -> Product c v
normalizeProduct = Product . filter ((/=0) . exponent)
                           . map merge
                           . groupBy (\x y -> base x == base y)
                           . sortOn base
                           . factors
  where
    merge fs = Factor (base . head $ fs) (sum $ exponent <$> fs)

prettyShowProduct :: (Show c, Show v) => Product c v -> String
prettyShowProduct = intercalate " " . map prettyShowFactor . factors

-- | Something like @-2 * x^2 * y^3@
data Term c v = Term { coeff   :: c,
                       product :: Product c v}
  deriving (Eq, Ord, Show)

instance Functor (Term c) where
    fmap f (Term c ps) = Term c (fmap f ps)

convertTerm :: (Integral c, Integral c', Ord v) => Term c v -> Term c' v
convertTerm (Term c p) = Term (fromIntegral c) (convertProduct p)

normalizeTerm :: (Integral c, Ord v) => Term c v -> Term c v
normalizeTerm (Term c p) = Term c (normalizeProduct p)

prettyShowTerm :: (Integral c, Show c, Show v) => Term c v -> String
prettyShowTerm (Term c (Product [])) = show c
prettyShowTerm (Term c p) | fromIntegral c == 1 = prettyShowProduct p
prettyShowTerm (Term c p) = show c ++ " " ++ prettyShowProduct p

termIsConst :: Term c v -> Bool
termIsConst = null . factors . product

scaleTerm :: (Integral c) => c -> Term c v -> Term c v
scaleTerm s (Term c p) = Term (s * c) p

--
-- * Operations
--

addFormula :: (Integral c, Ord v) => Formula c v-> Formula c v -> Formula c v
addFormula a b = normalize $ addFormula' a b

subFormula :: (Integral c, Ord v) => Formula c v-> Formula c v -> Formula c v
subFormula a b = normalize $ addFormula' a (scaleFormula' (-1) b)

sumFormula :: (Integral c, Ord v) => [Formula c v] -> Formula c v
sumFormula = normalize . foldr1 addFormula'

scaleFormula :: (Integral c, Ord v) => c -> Formula c v -> Formula c v
scaleFormula s = normalize . scaleFormula' s

mulFormula :: (Integral c, Ord v) => Formula c v -> Formula c v -> Formula c v
mulFormula a b = normalize $ mulFormula' a b

productFormula :: (Integral c, Ord v) => [Formula c v] -> Formula c v
productFormula = normalize . foldr1 mulFormula'

divFormula :: (Integral c, Ord v) => Formula c v -> Formula c v -> Formula c v
divFormula x y = normalize $ divFormula' x y

modFormula :: (Integral c, Ord v) => Formula c v -> Formula c v -> Formula c v
modFormula x y = normalize $ modFormula' x y

--
-- * Utility
--

addFormula' :: (Integral c) => Formula c v-> Formula c v -> Formula c v
addFormula' (Formula as) (Formula bs) = Formula (as ++ bs)

scaleFormula' :: (Integral c) => c -> Formula c v -> Formula c v
scaleFormula' s = Formula . map (scaleTerm s) . terms

mulFormula' :: (Integral c) => Formula c v -> Formula c v -> Formula c v
mulFormula' (Formula as) (Formula bs) = Formula . concat $ terms'
  where
    terms' = mulTerms <$> as <*> pure bs

mulTerm :: (Integral c) => Term c v -> Term c v -> Term c v
mulTerm (Term a b) (Term c d) = Term (a * c) (Product $ factors b ++ factors d)

mulTerms :: (Integral c) => Term c v -> [Term c v] -> [Term c v]
mulTerms t = map (mulTerm t)

divTerm :: (Integral c) => c -> Term c v -> Term c v
divTerm s (Term c p) = Term (c `div` s) p

canDivide :: (Integral c) => Formula c v -> Formula c v -> Bool
canDivide _ y | isZero y        = False
canDivide _ y | not (isConst y) = False
canDivide x y = all ((== 0) . (`mod` c) . coeff) $ terms x
  where
    c = fromJust $ toConstant y

divFormula' :: (Integral c) => Formula c v -> Formula c v -> Formula c v
divFormula' _ y | isZero y        = error "division by zero"
divFormula' _ y | not (isConst y) = error "division by non-const not implemented"
divFormula' x y = Formula $ map (divTerm c) $ terms x
  where
    c = fromJust $ toConstant y

modTerm :: (Integral c) => c -> Term c v -> Term c v
modTerm s (Term c p) = Term (c `mod` s) p

modFormula' :: (Integral c) => Formula c v -> Formula c v -> Formula c v
modFormula' _ y | isZero y        = error "modulo by zero"
modFormula' _ y | not (isConst y) = error "modulo by non-const not implemented"
modFormula' x y = Formula $ map (modTerm c) $ terms x
  where
    c = fromJust $ toConstant y

--
-- * Numeric Order
--

data NumOrdering = NumLT | NumEQ | NumGT | Sometimes
  deriving (Eq, Ord, Enum, Read, Show)

class NumOrd a where
    numCompare :: a -> a -> NumOrdering

fromOrdering :: Ordering -> NumOrdering
fromOrdering LT = NumLT
fromOrdering EQ = NumEQ
fromOrdering GT = NumGT

instance (Integral c, Ord v) => NumOrd (Formula c v) where
    numCompare a b =
        if diff == zero
            then NumEQ
            else if not (isConst diff)
                then Sometimes
                else fromOrdering $ compare (coeff . head . terms $ diff) 0
      where
        diff = addFormula a (scaleFormula' (-1) b)