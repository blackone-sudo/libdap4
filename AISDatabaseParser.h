
// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2003 OPeNDAP, Inc.
// Author: James Gallagher <jgallagher@opendap.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#ifndef ais_database_parser_h
#define ais_database_parser_h

#include <string>

#include <libxml/parserInternals.h>

#ifndef _internal_err_h
#include "InternalErr.h"
#endif

#ifndef ais_exceptions_h
#include "AISExceptions.h"
#endif

#ifndef ais_resources_h
#include "AISResources.h"
#endif

/** Parse the XML database/configuration file which lists a collection of AIS
    resources. 

    Static methods are used as callbacks for the SAX parser. They do not
    throw exceptions because exceptions from within callbacks are not
    reliable or portable (although exceptions themselves are not 100%
    portable...). To signal errors, the methods record information in the
    AISParserState object. Once any of the error handlers (aisWarning,
    aisError or aisFatalError) is called, construction of an AISResources
    object ends even though the SAX parser still calls the various callback
    functions. The parse method throws an AISDatabaseReadFailed exception if
    an error was found.

    Note that this class uses the C++-supplied default definitions for the
    default and copy constructors as well as the destructor and assignment
    operator.

    This class should be extended so that the line number is added to error
    messages. 

    @see AISResource */
class AISDatabaseParser {
private:
    /** States used by AISParserState. These are the states of the SAX parser
	state-machine. */ 
    enum ParseState {
	PARSER_START,
	PARSER_FINISH,
	AIS,
	ENTRY,
	PRIMARY,
	ANCILLARY,
	PARSER_UNKNOWN,
	PARSER_ERROR
    };

    /** This holds the state information for the SAX parser that is used to
	intern the XML AIS database. The parser is designed to ignore unknown
	tags and attributes, so long as the input is well-formed. Note that a
	pointer to an AISResources object is part of the SAX parser state. As
	the XML input document is parsed, information is added to that
	object. Also note that an AISParserState object holds a pointer to
	the xmlParserCtxt which, in turn, holds a pointer to AISParserState
	(via its \c userData field). This cirular referencing is done because
	libxml2's SAX parser invokes the callbacks using just the
	AISParserState instance but we need the whole xmlParserCtxt for some
	of the callbacks. 

	@see aisWarning. */
    struct AISParserState {
	ParseState state;	// current state
	ParseState prev_state;	// previous state
	int unknown_depth;	// handle recursive unknown tags
 
	string error_msg;	// Error message(s), if any.

	xmlParserCtxtPtr ctxt;	// used for error msg line numbers
	AISResources *ais; 	// dump info here

	string primary;		// current entry's primary URL/Regexp
	bool regexp;		// True if primary is a regexp

	ResourceVector rv;	// add ancillary entries to rv
    };
    
public:
    void intern(const string &database, AISResources *ais)
	throw(AISDatabaseReadFailed);

    static void aisStartDocument(AISParserState *state);
    static void aisEndDocument(AISParserState *state);
    static void aisStartElement(AISParserState *state, const char *name, 
				const char **attrs);
    static void aisEndElement(AISParserState *state, const char *name);
    static xmlEntityPtr aisGetEntity(AISParserState *state,
				     const xmlChar *name);
    static void aisWarning(AISParserState *state, const char *msg, ...);
    static void aisError(AISParserState *state, const char *msg, ...);
    static void aisFatalError(AISParserState *state, const char *msg, ...);
};

// $Log: AISDatabaseParser.h,v $
// Revision 1.3  2003/03/12 01:07:34  jimg
// Added regular expressions to the AIS subsystem. In an AIS database (XML)
// it is now possible to list a regular expression in place of an explicit
// URL. The AIS will try to match this Regexp against candidate URLs and
// return the ancillary resources for all those that succeed.
//
// Revision 1.2  2003/02/26 01:27:49  jimg
// Changed the name of the parse() method to intern().
//
// Revision 1.1  2003/02/20 22:06:50  jimg
// Added.
//

#endif // ais_database_parser_h