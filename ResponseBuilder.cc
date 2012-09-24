// -*- mode: c++; c-basic-offset:4 -*-

// This file is part of libdap, A C++ implementation of the OPeNDAP Data
// Access Protocol.

// Copyright (c) 2011 OPeNDAP, Inc.
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

#include "config.h"

#include <signal.h>
#include <unistd.h>

#ifndef WIN32
#include <sys/wait.h>
#else
#include <io.h>
#include <fcntl.h>
#include <process.h>
#endif

#include <iostream>
#include <string>
#include <sstream>
#include <cstring>

#include <uuid/uuid.h>	// used to build CID header value for data ddx
#include "DAS.h"
#include "DDS.h"
#include "debug.h"
#include "mime_util.h"	// for last_modified_time() and rfc_822_date()
#include "escaping.h"
#include "util.h"
#include "ResponseBuilder.h"
#include "XDRStreamMarshaller.h"
#include "DAP4streamMarshaller.h"

#ifndef WIN32
#include "SignalHandler.h"
#include "EventHandler.h"
#include "AlarmHandler.h"
#endif

#define CRLF "\r\n"             // Change here, expr-test.cc
using namespace std;

namespace libdap {

ResponseBuilder::~ResponseBuilder()
{
}

/** Called when initializing a ResponseBuilder that's not going to be passed
 command line arguments. */
void ResponseBuilder::initialize()
{
    // Set default values. Don't use the C++ constructor initialization so
    // that a subclass can have more control over this process.
    d_dataset = "";
    d_ce = "";
    d_timeout = 0;

    d_default_protocol = DAP_PROTOCOL_VERSION;
}

/** Return the entire constraint expression in a string.  This
 includes both the projection and selection clauses, but not the
 question mark.

 @brief Get the constraint expression.
 @return A string object that contains the constraint expression. */
string ResponseBuilder::get_ce() const
{
    return d_ce;
}

void ResponseBuilder::set_ce(string _ce)
{
    d_ce = www2id(_ce, "%", "%20");
}

/** The ``dataset name'' is the filename or other string that the
 filter program will use to access the data. In some cases this
 will indicate a disk file containing the data.  In others, it
 may represent a database query or some other exotic data
 access method.

 @brief Get the dataset name.
 @return A string object that contains the name of the dataset. */
string ResponseBuilder::get_dataset_name() const
{
    return d_dataset;
}

void ResponseBuilder::set_dataset_name(const string ds)
{
    d_dataset = www2id(ds, "%", "%20");
}

/** Set the server's timeout value. A value of zero (the default) means no
 timeout.

 @param t Server timeout in seconds. Default is zero (no timeout). */
void ResponseBuilder::set_timeout(int t)
{
    d_timeout = t;
}

/** Get the server's timeout value. */
int ResponseBuilder::get_timeout() const
{
    return d_timeout;
}

/** Use values of this instance to establish a timeout alarm for the server.
 If the timeout value is zero, do nothing.

 @todo When the alarm handler is called, two CRLF pairs are dumped to the
 stream and then an Error object is sent. No attempt is made to write the
 'correct' MIME headers for an Error object. Instead, a savvy client will
 know that when an exception is thrown during a deserialize operation, it
 should scan ahead in the input stream for an Error object. Add this, or a
 sensible variant once libdap++ supports reliable error delivery. Dumb
 clients will never get the Error object... */
void ResponseBuilder::establish_timeout(ostream &stream) const
{
#ifndef WIN32
    if (d_timeout > 0) {
        SignalHandler *sh = SignalHandler::instance();
        EventHandler *old_eh = sh->register_handler(SIGALRM, new AlarmHandler(stream));
        delete old_eh;
        alarm(d_timeout);
    }
#endif
}

/** This function formats and prints an ASCII representation of a
 DAS on stdout.  This has the effect of sending the DAS object
 back to the client program.

 @note This is the DAP2 attribute response.

 @brief Transmit a DAS.
 @param out The output stream to which the DAS is to be sent.
 @param das The DAS object to be sent.
 @param anc_location The directory in which the external DAS file resides.
 @param with_mime_headers If true (the default) send MIME headers.
 @return void
 @see DAS */
void ResponseBuilder::send_das(ostream &out, DAS &das, bool with_mime_headers) const
{
    if (with_mime_headers)
        set_mime_text(out, dods_das, x_plain, last_modified_time(d_dataset), "2.0");
    das.print(out);

    out << flush;
}

/** This function formats and prints an ASCII representation of a
 DDS on stdout.  When called by a CGI program, this has the
 effect of sending a DDS object back to the client
 program. Either an entire DDS or a constrained DDS may be sent.

 @note This is the DAP2 syntactic metadata response.

 @brief Transmit a DDS.
 @param out The output stream to which the DAS is to be sent.
 @param dds The DDS to send back to a client.
 @param eval A reference to the ConstraintEvaluator to use.
 @param constrained If this argument is true, evaluate the
 current constraint expression and send the `constrained DDS'
 back to the client.
 @param anc_location The directory in which the external DAS file resides.
 @param with_mime_headers If true (default) send MIME headers.
 @return void
 @see DDS */
void ResponseBuilder::send_dds(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool constrained,
        bool with_mime_headers) const
{
    // If constrained, parse the constraint. Throws Error or InternalErr.
    if (constrained)
        eval.parse_constraint(d_ce, dds);

    if (eval.functional_expression())
        throw Error(
                "Function calls can only be used with data requests. To see the structure of the underlying data source, reissue the URL without the function.");

    if (with_mime_headers)
        set_mime_text(out, dods_dds, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

    if (constrained)
        dds.print_constrained(out);
    else
        dds.print(out);

    out << flush;
}

/**
 * Build/return the BLOB part of the DAP2 data response.
 */
void ResponseBuilder::dataset_constraint(ostream &out, DDS & dds, ConstraintEvaluator & eval, bool ce_eval) const
{
    // send constrained DDS
    dds.print_constrained(out);
    out << "Data:\n";
    out << flush;
    XDRStreamMarshaller m(out);

    // Send all variables in the current projection (send_p())
    for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++)
        if ((*i)->send_p()) {
            DBG(cerr << "Sending " << (*i)->name() << endl);
            (*i)->serialize(eval, dds, m, ce_eval);
        }
}

/**
 * Build/return the DDX and the BLOB part of the DAP4 data response.
 */
void ResponseBuilder::dataset_constraint_ddx(ostream &out, DDS & dds, ConstraintEvaluator & eval,
        const string &boundary, const string &start, bool ce_eval) const
{
    // Write the MPM headers for the DDX (text/xml) part of the response
    set_mime_ddx_boundary(out, boundary, start);

    // Make cid
    uuid_t uu;
    uuid_generate(uu);
    char uuid[37];
    uuid_unparse(uu, &uuid[0]);
    char domain[256];
    if (getdomainname(domain, 255) != 0 || strlen(domain) == 0)
        strncpy(domain, "opendap.org", 255);

    string cid = string(&uuid[0]) + "@" + string(&domain[0]);

    // Send constrained DDX with a data blob reference
    dds.print_xml_writer(out, true, cid);

    // Grab a stream that encodes for DAP4
    DAP4StreamMarshaller m(out);

    // Write the MPM headers for the data part of the response.
    set_mime_data_boundary(out, boundary, cid, m.get_endian(), 0);

    // Send all variables in the current projection (send_p()). In DAP4,
    // all of the top-level variables are serialized with their checksums.
    // Internal variables are not.
    // TODO When Group support is added to libdap, this will need to be
    // generalized so that all variables in the top-levels of all the
    // groups will have checksums included in the response.
    for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++) {
        if ((*i)->send_p()) {
            DBG(cerr << "Sending " << (*i)->name() << endl);

            m.reset_checksum();

            (*i)->serialize(eval, dds, m, ce_eval);

            m.put_checksum();
        }
    }
}

/** Send the data in the DDS object back to the client program. The data is
 encoded using a Marshaller, and enclosed in a MIME document which is all sent
 to \c data_stream.

 @note This is the DAP2 data response.

 @brief Transmit data.
 @param dds A DDS object containing the data to be sent.
 @param eval A reference to the ConstraintEvaluator to use.
 @param data_stream Write the response to this stream.
 @param anc_location A directory to search for ancillary files (in
 addition to the CWD).  This is used in a call to
 get_data_last_modified_time().
 @param with_mime_headers If true, include the MIME headers in the response.
 Defaults to true.
 @return void */
void ResponseBuilder::send_data(ostream & data_stream, DDS & dds, ConstraintEvaluator & eval,
        bool with_mime_headers) const
{
    // Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
        string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                + "KB is too large; requests for this user are limited to "
                + long_to_string(dds.get_response_limit() / 1024) + "KB.";
        throw Error(msg);
    }

    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
        DDS *fdds = eval.eval_function_clauses(dds);
        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dataset_constraint(data_stream, *fdds, eval, false);
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_binary(data_stream, dods_data, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

        dataset_constraint(data_stream, dds, eval);
    }

    data_stream << flush;
}

/** Send the DDX response. The DDX never contains data, instead it holds a
 reference to a Blob response which is used to get the data values. The
 DDS and DAS objects are built using code that already exists in the
 servers.

 @note This is the DAP4 metadata response; it is supported by most DAP2
 servers as well, although the DAP4 DDX will contain types not present in
 DAP2.

 @param dds The dataset's DDS \e with attributes in the variables.
 @param eval A reference to the ConstraintEvaluator to use.
 @param out Destination
 @param with_mime_headers If true, include the MIME headers in the response.
 Defaults to true. */
void ResponseBuilder::send_ddx(ostream &out, DDS &dds, ConstraintEvaluator &eval, bool with_mime_headers) const
{
    // If constrained, parse the constraint. Throws Error or InternalErr.
    if (!d_ce.empty())
        eval.parse_constraint(d_ce, dds);

    if (eval.functional_expression())
        throw Error(
                "Function calls can only be used with data requests. To see the structure of the underlying data source, reissue the URL without the function.");

    if (with_mime_headers)
        set_mime_text(out, dap4_ddx, x_plain, last_modified_time(d_dataset), dds.get_dap_version());

    dds.print_xml_writer(out, !d_ce.empty(), "");
}

/** Send the data in the DDS object back to the client program. The data is
 encoded using a Marshaller, and enclosed in a MIME document which is all sent
 to \c data_stream.

 @note This is the DAP4 data response.

 @brief Transmit data.
 @param dds A DDS object containing the data to be sent.
 @param eval A reference to the ConstraintEvaluator to use.
 @param data_stream Write the response to this stream.
 @param anc_location A directory to search for ancillary files (in
 addition to the CWD).  This is used in a call to
 get_data_last_modified_time().
 @param with_mime_headers If true, include the MIME headers in the response.
 Defaults to true.
 @return void */
void ResponseBuilder::send_data_ddx(ostream & data_stream, DDS & dds, ConstraintEvaluator & eval, const string &start,
        const string &boundary, bool with_mime_headers) const
{
    // Set up the alarm.
    establish_timeout(data_stream);
    dds.set_timeout(d_timeout);

    eval.parse_constraint(d_ce, dds); // Throws Error if the ce doesn't parse.

    if (dds.get_response_limit() != 0 && dds.get_request_size(true) > dds.get_response_limit()) {
        string msg = "The Request for " + long_to_string(dds.get_request_size(true) / 1024)
                + "KB is too large; requests for this user are limited to "
                + long_to_string(dds.get_response_limit() / 1024) + "KB.";
        throw Error(msg);
    }

    dds.tag_nested_sequences(); // Tag Sequences as Parent or Leaf node.

    // Start sending the response...

    // Handle *functional* constraint expressions specially
    if (eval.function_clauses()) {
        // We could unique_ptr<DDS> here to avoid memory leaks if
        // dataset_constraint_ddx() throws an exception.
        DDS *fdds = eval.eval_function_clauses(dds);
        try {
            if (with_mime_headers)
                set_mime_multipart(data_stream, boundary, start, x_plain, last_modified_time(d_dataset));
            data_stream << flush;
            dataset_constraint_ddx(data_stream, *fdds, eval, boundary, start);
        }
        catch (...) {
            delete fdds;
            throw;
        }
        delete fdds;
    }
    else {
        if (with_mime_headers)
            set_mime_multipart(data_stream, boundary, start, x_plain, last_modified_time(d_dataset));
        data_stream << flush;
        dataset_constraint_ddx(data_stream, dds, eval, boundary, start);
    }

    data_stream << flush;

    if (with_mime_headers)
        data_stream << CRLF << "--" << boundary << "--" << CRLF;
}

/**
 * Send the DAP4 DMR (Dataset Metadata Response)
 *
 * @note The DAP2/3 methods have an optional 'with_mime_headers' parameter
 * that triggers the generation of a complete HTTP response document. This
 * method lacks that.
 *
 * @todo Modify the definition of server-functions so that they can return
 * the DMR.
 */
void
ResponseBuilder::send_dmr(ostream &out, DDS &dds, ConstraintEvaluator &eval) const
{
    // If constrained, parse the constraint. Throws Error or InternalErr.
    if (!d_ce.empty())
        eval.parse_constraint(d_ce, dds);

    // TODO Change functions so this is no longer an error
    if (eval.functional_expression())
        throw Error(
                "Function calls can only be used with data requests. To see the structure of the underlying data source, reissue the URL without the function.");

    dds.print_dmr(out, !d_ce.empty());
}
/**
 * Build a DAP4 data response document body and write it to the output
 * stream 'out'.
 *
 * @note The DAP2/3 methods have an optional 'with_mime_headers' parameter
 * that triggers the generation of a complete HTTP response document. This
 * method lacks that.
 *
 * @param out Write the response body here
 * @param dds This DDS holds the variables to serialize
 * @parma eval Use this instance of the CE evaluator to subset/sample the
 * dataset
 */
void
ResponseBuilder::send_dap4_data(ostream &out, DDS &dds, ConstraintEvaluator &eval) const
{
    throw InternalErr(__FILE__, __LINE__, "ResponseBuilder::send_dap4_data: Not implemented");

    // TODO
    // Print the chunk offset info so that clients can skip the DMR and go
    // directly to the data.

    // Send constrained DMR
    dds.print_dmr(out, !d_ce.empty());

    // Grab a stream that encodes for DAP4
    DAP4StreamMarshaller m(out);

    // TODO Write word order information

    // Send all variables in the current projection (send_p()). In DAP4,
    // all of the top-level variables are serialized with their checksums.
    // Internal variables are not.
    //
    // TODO When Group support is added to libdap, this will need to be
    // generalized so that all variables in the top-levels of all the
    // groups will have checksums included in the response.
    //
    // TODO Switch to the DAP4 serialization method once it's written
    for (DDS::Vars_iter i = dds.var_begin(); i != dds.var_end(); i++) {
        if ((*i)->send_p()) {
            DBG(cerr << "Sending " << (*i)->name() << endl);

            m.reset_checksum();

            // FIXME Replace with DAP4 call
            (*i)->serialize(eval, dds, m, true);

            m.put_checksum();
        }
    }

}

static const char *descrip[] = { "unknown", "dods_das", "dods_dds", "dods_data", "dods_error", "web_error", "dap4-ddx",
        "dap4-data", "dap4-error", "dap4-data-ddx", "dods_ddx" };
static const char *encoding[] = { "unknown", "deflate", "x-plain", "gzip", "binary" };

/** Generate an HTTP 1.0 response header for a text document. This is used
 when returning a serialized DAS or DDS object.

 @note In Hyrax these headers are not used. Instead the front end of the
 server will build the response headers

 @param strm Write the MIME header to this stream.
 @param type The type of this this response. Defaults to
 application/octet-stream.
 @param ver The version string; denotes the libdap implementation
 version.
 @param enc How is this response encoded? Can be plain or deflate or the
 x_... versions of those. Default is x_plain.
 @param last_modified The time to use for the Last-Modified header value.
 Default is zero which means use the current time. */
void ResponseBuilder::set_mime_text(ostream &strm, ObjectType type, EncodingType enc, const time_t last_modified,
        const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR<< CRLF;
    strm << "XOPeNDAP-Server: " << DVR<< CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    if (type == dap4_ddx)
        strm << "Content-Type: text/xml" << CRLF;
    else
        strm << "Content-Type: text/plain" << CRLF;

    // Note that Content-Description is from RFC 2045 (MIME, pt 1), not 2616.
    // jhrg 12/23/05
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (type == dods_error) // don't cache our error responses.
        strm << "Cache-Control: no-cache" << CRLF;
    // Don't write a Content-Encoding header for x-plain since that breaks
    // Netscape on NT. jhrg 3/23/97
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;
    strm << CRLF;
}

/** Generate an HTTP 1.0 response header for a html document.

 @param strm Write the MIME header to this stream.
 @param type The type of this this response.
 @param ver The version string; denotes the libdap implementation
 version.
 @param enc How is this response encoded? Can be plain or deflate or the
 x_... versions of those. Default is x_plain.
 @param last_modified The time to use for the Last-Modified header value.
 Default is zero which means use the current time. */
void ResponseBuilder::set_mime_html(ostream &strm, ObjectType type, EncodingType enc, const time_t last_modified,
        const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR<< CRLF;
    strm << "XOPeNDAP-Server: " << DVR<< CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    strm << "Content-type: text/html" << CRLF;
    // See note above about Content-Description header. jhrg 12/23/05
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (type == dods_error) // don't cache our error responses.
        strm << "Cache-Control: no-cache" << CRLF;
    // Don't write a Content-Encoding header for x-plain since that breaks
    // Netscape on NT. jhrg 3/23/97
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;
    strm << CRLF;
}

/** Write an HTTP 1.0 response header for our binary response document (i.e.,
 the DataDDS object).

 @param strm Write the MIME header to this stream.
 @param type The type of this this response. Defaults to
 application/octet-stream.
 @param ver The version string; denotes the libdap implementation
 version.
 @param enc How is this response encoded? Can be plain or deflate or the
 x_... versions of those. Default is x_plain.
 @param last_modified The time to use for the Last-Modified header value.
 Default is zero which means use the current time.
 */
void ResponseBuilder::set_mime_binary(ostream &strm, ObjectType type, EncodingType enc, const time_t last_modified,
        const string &protocol) const
{
    strm << "HTTP/1.0 200 OK" << CRLF;

    strm << "XDODS-Server: " << DVR<< CRLF;
    strm << "XOPeNDAP-Server: " << DVR<< CRLF;

    if (protocol == "")
        strm << "XDAP: " << d_default_protocol << CRLF;
    else
        strm << "XDAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    strm << "Content-Type: application/octet-stream" << CRLF;
    strm << "Content-Description: " << descrip[type] << CRLF;
    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;

    strm << CRLF;
}

/** Build the initial headers for the DAP4 data response */

void ResponseBuilder::set_mime_multipart(ostream &strm, const string &boundary, const string &start, EncodingType enc,
        const time_t last_modified, const string &protocol, const string &url) const
{
    strm << "HTTP/1.1 200 OK" << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;

    strm << "Last-Modified: ";
    if (last_modified > 0)
        strm << rfc822_date(last_modified).c_str() << CRLF;
    else
        strm << rfc822_date(t).c_str() << CRLF;

    strm << "Content-Type: multipart/related; boundary=" << boundary << "; start=\"<" << start
            << ">\"; type=\"text/xml\"" << CRLF;

    strm << "Content-Description: data-ddx;";
    if (!url.empty())
        strm << " url=\"" << url << "\"" << CRLF;
    else
        strm << CRLF;

    if (enc != x_plain)
        strm << "Content-Encoding: " << encoding[enc] << CRLF;

    if (protocol == "")
        strm << "X-DAP: " << d_default_protocol << CRLF;
    else
        strm << "X-DAP: " << protocol << CRLF;

    strm << "X-OPeNDAP-Server: " << DVR<< CRLF;

    strm << CRLF;
}

void ResponseBuilder::set_mime_ddx_boundary(ostream &strm, const string &boundary, const string &cid) const
{
    strm << "--" << boundary << CRLF;
    strm << "Content-Type: text/xml; charset=UTF-8" << CRLF;
    strm << "Content-Transfer-Encoding: binary" << CRLF;
    strm << "Content-Description: ddx" << CRLF;
    strm << "Content-Id: <" << cid << ">" << CRLF;

    strm << CRLF;
}

void ResponseBuilder::set_mime_data_boundary(ostream &strm, const string &boundary, const string &cid,
        const string &endian, unsigned long long len) const
{
    strm << "--" << boundary << CRLF;
    strm << "Content-Type: application/x-dap-" << endian << "-endian" << CRLF;
    strm << "Content-Transfer-Encoding: binary" << CRLF;
    strm << "Content-Description: data" << CRLF;
    strm << "Content-Id: <" << cid << ">" << CRLF;
    strm << "Content-Length: " << len << CRLF;

    strm << CRLF;
}

/** Generate an HTTP 1.0 response header for an Error object.
 @param strm Write the MIME header to this stream.
 @param code HTTP 1.0 response code. Should be 400, ... 500, ...
 @param reason Reason string of the HTTP 1.0 response header.
 @param version The version string; denotes the DAP spec and implementation
 version. */
void ResponseBuilder::set_mime_error(ostream &strm, int code, const string &reason, const string &protocol) const
{
    strm << "HTTP/1.0 " << code << " " << reason.c_str() << CRLF;

    strm << "XDODS-Server: " << DVR<< CRLF;
    strm << "X-OPeNDAP-Server: " << DVR<< CRLF;

    if (protocol == "")
        strm << "X-DAP: " << d_default_protocol << CRLF;
    else
        strm << "X-DAP: " << protocol << CRLF;

    const time_t t = time(0);
    strm << "Date: " << rfc822_date(t).c_str() << CRLF;
    strm << "Cache-Control: no-cache" << CRLF;
    strm << CRLF;
}

} // namespace libdap

