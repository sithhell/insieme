/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#include <gtest/gtest.h>
#include <xml_utils.h>
#include <xercesc/util/XercesDefs.hpp>

using namespace std;
using namespace insieme::core;
using namespace insieme::xml;

/*TEST(XmlTest, GenericTypeTest) {
	NodeManager manager;

	GenericTypePtr type1 = GenericType::get(manager, "int");
	GenericTypePtr type2 = GenericType::get(manager, "int", toVector<TypePtr>(type1, type1), toVector(IntTypeParam::getVariableIntParam('p')), type1);
	
	NodePtr root = type1;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}*/

TEST(XmlTest, FunctionTypeTest) {
	NodeManager manager;
	
	GenericTypePtr type1 = GenericType::get(manager, "int");
	GenericTypePtr type2 = GenericType::get(manager, "int", toVector<TypePtr>(type1, type1), toVector(IntTypeParam::getVariableIntParam('p')), type1);
	
	FunctionTypePtr funType1 = FunctionType::get(manager, type1, type2);
	
	NodePtr root = funType1;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}

TEST(XmlTest, StructTypeTest) {
	NodeManager manager;

	Identifier identA("a");
	Identifier identB("b");

	StructType::Entries entriesA;
	entriesA.push_back(StructType::Entry(identA, GenericType::get(manager, "A")));
	entriesA.push_back(StructType::Entry(identB, GenericType::get(manager, "B")));

	StructTypePtr structA = StructType::get(manager, entriesA);
	
	NodePtr root = structA;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}

TEST(XmlTest, UnionTypeTest) {
	NodeManager manager;

	Identifier identA("a");
	Identifier identB("b");

	UnionType::Entries entriesA;
	entriesA.push_back(UnionType::Entry(identA, GenericType::get(manager, "A")));
	entriesA.push_back(UnionType::Entry(identB, GenericType::get(manager, "B")));

	UnionTypePtr unionA = UnionType::get(manager, entriesA);
	
	NodePtr root = unionA;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}

TEST(XmlTest, TupleTypeTest) {
	NodeManager manager;

	vector<TypePtr> subTypesA;
	subTypesA.push_back(GenericType::get(manager, "int"));
	subTypesA.push_back(GenericType::get(manager, "val"));

	TupleTypePtr tupleA = TupleType::get(manager, subTypesA);
	
	NodePtr root = tupleA;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}

TEST(XmlTest, TypeVariableTest) {
	NodeManager manager;
	TypeVariablePtr varTypeA = TypeVariable::get(manager, "alpha");
	TypeVariablePtr varTypeB = TypeVariable::get(manager, "beta");
	TypeVariablePtr varTypeG = TypeVariable::get(manager, "gamma");
	GenericTypePtr type1 = GenericType::get(manager, "int", toVector<TypePtr>(varTypeA, varTypeB), toVector(IntTypeParam::getVariableIntParam('p')), varTypeG);
	
	NodePtr root = type1;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}

TEST(XmlTest, RecTypeTest) {
	NodeManager manager;
	TypeVariablePtr varX = TypeVariable::get(manager, "X");
	GenericTypePtr gen = GenericType::get(manager, "int");
	RecTypeDefinition::RecTypeDefs definitions;
	definitions.insert(std::make_pair(varX, gen));
	
	RecTypeDefinitionPtr definition = RecTypeDefinition::get(manager, definitions);
 
	RecTypePtr type = RecType::get(manager, varX, definition);
	
	NodePtr root = type;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}


// ------------------- DummyAnnotation ---------------------------------
class DummyAnnotation : public Annotation {
public:
	static StringKey<DummyAnnotation> DummyKey;
	string value;
	DummyAnnotation(string value) : value(value) { };

	virtual AnnotationKey* getKey() const {
		return &DummyKey;
	}
	
	const std::string getAnnotationName() const {
		 return "DummyAnnotation"; 
	}
};

// initalization of the dummy key
StringKey<DummyAnnotation> DummyAnnotation::DummyKey("DummyKey");

XmlElement DummyAnnotationToXML(DummyAnnotation ann, XmlElement el){
	XmlElement intNode("int", el.getDoc());
	intNode.setText(ann.value);
	el << intNode;
	return el;
}

shared_ptr<Annotation> DummyAnnotationFromXML(XmlElement el){
	return shared_ptr<Annotation> (new DummyAnnotation("1"));
}

XML_CONVERTER(DummyAnnotation, DummyAnnotationToXML, DummyAnnotationFromXML)

typedef shared_ptr<DummyAnnotation> DummyAnnotationPtr;

// ------------------- VectorAnnotation ---------------------------------
class VectorAnnotation : public Annotation {
public:
	static StringKey<VectorAnnotation> VectorKey;
	vector<string> values;
	VectorAnnotation(vector<string> values) : values(values) { };

	virtual AnnotationKey* getKey() const {
		return &VectorKey;
	}
	
	const std::string getAnnotationName() const {
		 return "VectorAnnotation"; 
	}
};

// initalization of the vector key
StringKey<VectorAnnotation> VectorAnnotation::VectorKey("VectorKey");

XmlElement VectorAnnotationToXML(VectorAnnotation ann, XmlElement el){
	XmlElement entries("entries", el.getDoc());
	el << entries;
	for (vector<string>::const_iterator iter = ann.values.begin(); iter != ann.values.end(); ++iter){
		XmlElement entry("entry", el.getDoc());
		entry.setText(*iter);
		entries << entry;
	}
	
	vector<XmlElement> prova = entries.getChildren();
	for (vector<XmlElement>::const_iterator iter = prova.begin(); iter != prova.end(); ++iter){
		std::cout << iter->getName() << std::endl;
		std::cout << iter->getText() << std::endl;
	}
	
	return el;
}

shared_ptr<Annotation> VectorAnnotationFromXML(XmlElement el){
	vector <string> vec;
	vec.push_back("test1");
	vec.push_back("test2");
	return shared_ptr<Annotation> (new VectorAnnotation(vec));
}

XML_CONVERTER(VectorAnnotation, VectorAnnotationToXML, VectorAnnotationFromXML)

typedef shared_ptr<VectorAnnotation> VectorAnnotationPtr;




TEST(XmlTest, GenericTypeAnnotationTest) {
	vector <string> vec;
	vec.push_back("genTy e1");
	vec.push_back("genTy e2");
	
	VectorAnnotationPtr vector_gte(new VectorAnnotation(vec));
	DummyAnnotationPtr dummy_gtn(new DummyAnnotation("genTy n"));
	DummyAnnotationPtr dummy_be(new DummyAnnotation("base e"));
	DummyAnnotationPtr dummy_bn(new DummyAnnotation("base n"));
	DummyAnnotationPtr dummy_tp1e(new DummyAnnotation("typePar1 e"));
	DummyAnnotationPtr dummy_tp1n(new DummyAnnotation("typePar1 n"));
	DummyAnnotationPtr dummy_tp2e(new DummyAnnotation("typePar2 e"));
	DummyAnnotationPtr dummy_tp2n(new DummyAnnotation("typePar2 n"));
	
	NodeManager manager;
	GenericTypePtr type1 = GenericType::get(manager, "type1");
	type1.addAnnotation(dummy_tp1e);
	type1->addAnnotation(dummy_tp1n);
	GenericTypePtr type2 = GenericType::get(manager, "type2");
	type2.addAnnotation(dummy_tp2e);
	type2->addAnnotation(dummy_tp2n);
	GenericTypePtr type3 = GenericType::get(manager, "type3");
	type3.addAnnotation(dummy_be);
	type3->addAnnotation(dummy_bn);
	GenericTypePtr type4 = GenericType::get(manager, "int", toVector<TypePtr>(type1, type2), toVector(IntTypeParam::getVariableIntParam('p')), type3);
	type4.addAnnotation(vector_gte);
	type4->addAnnotation(dummy_gtn);
	
	NodePtr root = type4;
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}

/* OK TEST(XmlTest, FunctionTypeAnnotationTest) {
	NodeManager manager;
	
	GenericTypePtr type1 = GenericType::get(manager, "val");
	GenericTypePtr type2 = GenericType::get(manager, "int");

	DummyAnnotationPtr dummy_fe(new DummyAnnotation("fun e"));
	DummyAnnotationPtr dummy_fn(new DummyAnnotation("fun n"));
	DummyAnnotationPtr dummy_re(new DummyAnnotation("ret e"));
	DummyAnnotationPtr dummy_ae(new DummyAnnotation("arg e"));
	DummyAnnotationPtr dummy_rn(new DummyAnnotation("ret n"));
	DummyAnnotationPtr dummy_an(new DummyAnnotation("arg n"));
	
	type1.addAnnotation(dummy_ae);
	type1->addAnnotation(dummy_an);
	
	type2.addAnnotation(dummy_re);
	type2->addAnnotation(dummy_rn);
	
	FunctionTypePtr funType1 = FunctionType::get(manager, type1, type2);
	
	funType1.addAnnotation(dummy_fe);
	funType1->addAnnotation(dummy_fn);
	
	NodePtr root = funType1;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);
}*/

/*TEST(XmlTest, StructTypeAnnotationTest) {
	NodeManager manager;

	Identifier identA("a");
	Identifier identB("b");

	StructType::Entries entriesA;
	entriesA.push_back(StructType::Entry(identA, GenericType::get(manager, "A")));
	entriesA.push_back(StructType::Entry(identB, GenericType::get(manager, "B")));

	StructTypePtr structA = StructType::get(manager, entriesA);
	
	DummyAnnotationPtr dummy_se(new DummyAnnotation("struct e"));
	DummyAnnotationPtr dummy_sn(new DummyAnnotation("struct n"));
	
	DummyAnnotationPtr dummy_re(new DummyAnnotation("ret e"));
	DummyAnnotationPtr dummy_ae(new DummyAnnotation("arg e"));
	DummyAnnotationPtr dummy_rn(new DummyAnnotation("ret n"));
	DummyAnnotationPtr dummy_an(new DummyAnnotation("arg n"));
	structA.addAnnotation(dummy_se);
	structA->addAnnotation(dummy_sn);
	
	NodePtr root = structA;
	
	XmlUtil xml;
	xml.convertIrToDom(root);
	string s1 = xml.convertDomToString();
	xml.convertDomToXml("dump1.xml");
	xml.convertXmlToDom("dump1.xml", true);
	string s2 = xml.convertDomToString();
	EXPECT_EQ (s1, s2);	
}*/


