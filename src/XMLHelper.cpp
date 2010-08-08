/* XmlHelper.cpp : libXML2 wrapper code
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
#include "XMLHelper.h"

unsigned int XMLHelpNextType(xmlNodePtr &node, int nodeType)
{
	do
	{
		node= node->next;
		if(!node)
			return 1;
	}
	while(node->type != nodeType);
	return 0;
}

//returns zero on success, nonzero on fail
unsigned int XMLHelpFwdToElem(xmlNodePtr &node, const char *nodeName)
{
	do
	{	
		node=node->next;
	}while(node != NULL &&  
		xmlStrcmp(node->name,(const xmlChar *) nodeName));
	return (!node);
}

unsigned int XMLHelpFwdNotElem(xmlNodePtr &node,const char *nodeName)
{
	do
	{
		node=node->next;
	}while(node !=NULL && node->type != XML_ELEMENT_NODE && 
		!xmlStrcmp(node->name,(const xmlChar *)nodeName));
	
	return !node;
}

string XMLHelpGetText(xmlNodePtr node)
{
	string result;
	XMLHelpNextType(node,XML_TEXT_NODE);
	result =(char *) node->content;
	return result;
}


