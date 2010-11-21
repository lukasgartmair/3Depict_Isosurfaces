/* 
 * XMLHelper.h - libXML2 wrapper functions
 * Copyright (C) 2010  D Haley
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XMLHELPER_H
#define XMLHELPER_H

//Undefs are because wxwidgets and libxml both define this
#undef ATTRIBUTE_PRINTF
#include <libxml/xmlreader.h>
#undef ATTRIBUTE_PRINTF

#include <string>
using std::string;
#ifdef DEBUG
#include <iostream>
using namespace std;
#endif


//These functions return nonzero on failure,
//zero on success
//be warned that the node WILL be modified.

//Jump to next element that is of a given type (eg text, node, comment etc)
//see EOF for more details. Returns nonzero on error
unsigned int XMLHelpNextType(xmlNodePtr &node,int);
//Scroll forwards until we reach an element of a given node. return nonzero on error
unsigned int XMLHelpFwdToElem(xmlNodePtr &node,  const char *nodeName);
//scroll forwards until we reach an element that is NOT of a given type. return nonzero on error.
unsigned int XMLHelpFwdNotElem(xmlNodePtr &node,const char *nodeName);
//Grab the text associated with the node
string XMLHelpGetText(xmlNodePtr &node);


//!Jump to the next element of the given name and retreive the value for the specified attrip
//returns false on failure
//NOTE: Do not use if your value may validly contain whitespace. stream_cast skips these cases
template<class T> 
bool XMLGetNextElemAttrib(xmlNodePtr &nodePtr, T &v, const char *nodeName, const char *attrib)
{
	std::string tmpStr;
	xmlChar *xmlString;
	//====
	if(XMLHelpFwdToElem(nodePtr,nodeName))
		return false;

	xmlString=xmlGetProp(nodePtr,(const xmlChar *)attrib);
	if(!xmlString)
		return false;
	tmpStr=(char *)xmlString;

	if(stream_cast(v,tmpStr))
	{
		xmlFree(xmlString);
		return false;
	}

	xmlFree(xmlString);

	return true;
}


/* Defined in the bowels of the xmlLib2 library
 * Enum xmlElementType {
 *	XML_ELEMENT_NODE = 1
 *	XML_ATTRIBUTE_NODE = 2
 *	XML_TEXT_NODE = 3
 *	XML_CDATA_SECTION_NODE = 4
 *	XML_ENTITY_REF_NODE = 5
 *	XML_ENTITY_NODE = 6
 *	XML_PI_NODE = 7
 *	XML_COMMENT_NODE = 8
 *	XML_DOCUMENT_NODE = 9
 *	XML_DOCUMENT_TYPE_NODE = 10
 *	XML_DOCUMENT_FRAG_NODE = 11
 *	XML_NOTATION_NODE = 12
 *	XML_HTML_DOCUMENT_NODE = 13
 *	XML_DTD_NODE = 14
 *	XML_ELEMENT_DECL = 15
 *	XML_ATTRIBUTE_DECL = 16
 *	XML_ENTITY_DECL = 17
 *	XML_NAMESPACE_DECL = 18
 *	XML_XINCLUDE_START = 19
 *	XML_XINCLUDE_END = 20
 *	XML_DOCB_DOCUMENT_NODE = 21
 *	}
 */
#endif
		
