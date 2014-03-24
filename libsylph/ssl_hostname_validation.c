/*
 * Helper functions to perform basic hostname validation using OpenSSL.
 *
 * Copyright (C) 2012, iSEC Partners.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 + use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 + so, subject to the following conditions:
 +
 + The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Author:  Alban Diquet
 *
 * https://github.com/iSECPartners/ssl-conservatory
 *
 * Modified naming convention to match LibSylph.
 *
 */
 
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <glib.h>

#include <strings.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>

#include "utils.h"
#include "ssl_hostname_validation.h"


#define HOSTNAME_MAX_SIZE 255


/* The following host_match() function is based on cURL/libcurl code. */

/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2013, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

/***************************************************************************
 *
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright (c) 1996 - 2014, Daniel Stenberg, <daniel@haxx.se>.
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall not
 * be used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization of the copyright holder.
 *
 ***************************************************************************/

/**
* Match a hostname against a wildcard pattern.
* E.g.
*  "foo.host.com" matches "*.host.com".
*
* We use the matching rule described in RFC6125, section 6.4.3.
* http://tools.ietf.org/html/rfc6125#section-6.4.3
*/

static int host_match(const char *hostname, const char *pattern)
{
	const char *pattern_label_end, *pattern_wildcard, *hostname_label_end;
	size_t prefixlen, suffixlen;

	if (!pattern || !*pattern || !hostname || !*hostname)
		return SSL_HOSTNAME_MATCH_NOT_FOUND;

	/* trivial case */
	if (g_ascii_strcasecmp(pattern, hostname) == 0)
		return SSL_HOSTNAME_MATCH_FOUND;

	pattern_wildcard = strchr(pattern, '*');
	if (pattern_wildcard == NULL) {
		return g_ascii_strcasecmp(pattern, hostname) == 0 ?
			SSL_HOSTNAME_MATCH_FOUND : SSL_HOSTNAME_MATCH_NOT_FOUND;
	}

	/* We require at least 2 dots in pattern to avoid too wide wildcard
	   match. */
	pattern_label_end = strchr(pattern, '.');
	if (pattern_label_end == NULL ||
	    strchr(pattern_label_end + 1, '.') == NULL ||
	    pattern_wildcard > pattern_label_end ||
	    g_ascii_strncasecmp(pattern, "xn--", 4) == 0) {
		return g_ascii_strcasecmp(pattern, hostname) == 0 ?
		SSL_HOSTNAME_MATCH_FOUND : SSL_HOSTNAME_MATCH_NOT_FOUND;
	}

	hostname_label_end = strchr(hostname, '.');
	if (hostname_label_end == NULL ||
	    g_ascii_strcasecmp(pattern_label_end, hostname_label_end) != 0)
		return SSL_HOSTNAME_MATCH_NOT_FOUND;

	/* The wildcard must match at least one character, so the left-most
	   label of the hostname is at least as large as the left-most label
	   of the pattern. */
	if (hostname_label_end - hostname < pattern_label_end - pattern)
		return SSL_HOSTNAME_MATCH_NOT_FOUND;

	prefixlen = pattern_wildcard - pattern;
	suffixlen = pattern_label_end - (pattern_wildcard + 1);
	return g_ascii_strncasecmp(pattern, hostname, prefixlen) == 0 &&
	       g_ascii_strncasecmp(pattern_wildcard + 1, hostname_label_end - suffixlen, suffixlen) == 0 ?
		SSL_HOSTNAME_MATCH_FOUND : SSL_HOSTNAME_MATCH_NOT_FOUND;
}


/**
* Tries to find a match for hostname in the certificate's Common Name field.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if the Common Name had a NUL character embedded in it.
* Returns Error if the Common Name could not be extracted.
*/
static SSLHostnameValidationResult matches_common_name(const char *hostname, const X509 *server_cert) {
	int common_name_loc = -1;
	X509_NAME_ENTRY *common_name_entry = NULL;
	ASN1_STRING *common_name_asn1 = NULL;
	char *common_name_str = NULL;

	// Find the position of the CN field in the Subject field of the certificate
	common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name((X509 *) server_cert), NID_commonName, -1);
	if (common_name_loc < 0) {
		return SSL_HOSTNAME_ERROR;
	}

	// Extract the CN field
	common_name_entry = X509_NAME_get_entry(X509_get_subject_name((X509 *) server_cert), common_name_loc);
	if (common_name_entry == NULL) {
		return SSL_HOSTNAME_ERROR;
	}

	// Convert the CN field to a C string
	common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
	if (common_name_asn1 == NULL) {
		return SSL_HOSTNAME_ERROR;
	}			
	common_name_str = (char *) ASN1_STRING_data(common_name_asn1);

	debug_print("matches_common_name: %s\n", common_name_str);

	// Make sure there isn't an embedded NUL character in the CN
	if (ASN1_STRING_length(common_name_asn1) != strlen(common_name_str)) {
		return SSL_HOSTNAME_MALFORMED_CERTIFICATE;
	}

	// Compare expected hostname with the CN
	return host_match(hostname, common_name_str);
}


/**
* Tries to find a match for hostname in the certificate's Subject Alternative Name extension.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
* Returns NoSANPresent if the SAN extension was not present in the certificate.
*/
static SSLHostnameValidationResult matches_subject_alternative_name(const char *hostname, const X509 *server_cert) {
	SSLHostnameValidationResult result = SSL_HOSTNAME_MATCH_NOT_FOUND;
	int i;
	int san_names_nb = -1;
	STACK_OF(GENERAL_NAME) *san_names = NULL;

	// Try to extract the names within the SAN extension from the certificate
	san_names = X509_get_ext_d2i((X509 *) server_cert, NID_subject_alt_name, NULL, NULL);
	if (san_names == NULL) {
		return SSL_HOSTNAME_NO_SAN_PRESENT;
	}
	san_names_nb = sk_GENERAL_NAME_num(san_names);

	// Check each name within the extension
	for (i=0; i<san_names_nb; i++) {
		const GENERAL_NAME *current_name = sk_GENERAL_NAME_value(san_names, i);

		if (current_name->type == GEN_DNS) {
			// Current name is a DNS name, let's check it
			char *dns_name = (char *) ASN1_STRING_data(current_name->d.dNSName);

			debug_print("matches_subject_alternative_name: %s\n", dns_name);

			// Make sure there isn't an embedded NUL character in the DNS name
			if (ASN1_STRING_length(current_name->d.dNSName) != strlen(dns_name)) {
				result = SSL_HOSTNAME_MALFORMED_CERTIFICATE;
				break;
			}
			else { // Compare expected hostname with the DNS name
				if (host_match(hostname, dns_name) == SSL_HOSTNAME_MATCH_FOUND) {
					result = SSL_HOSTNAME_MATCH_FOUND;
					break;
				}
			}
		}
	}
	sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

	return result;
}


/**
* Validates the server's identity by looking for the expected hostname in the
* server's certificate. As described in RFC 6125, it first tries to find a match
* in the Subject Alternative Name extension. If the extension is not present in
* the certificate, it checks the Common Name instead.
*
* Returns MatchFound if a match was found.
* Returns MatchNotFound if no matches were found.
* Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
* Returns Error if there was an error.
*/
SSLHostnameValidationResult ssl_validate_hostname(const char *hostname, const X509 *server_cert) {
	SSLHostnameValidationResult result;

	debug_print("ssl_validate_hostname: validating hostname: %s\n", hostname);

	if((hostname == NULL) || (server_cert == NULL))
		return SSL_HOSTNAME_ERROR;

	// First try the Subject Alternative Names extension
	result = matches_subject_alternative_name(hostname, server_cert);
	if (result == SSL_HOSTNAME_NO_SAN_PRESENT) {
		// Extension was not found: try the Common Name
		result = matches_common_name(hostname, server_cert);
	}

	return result;
}

#endif /* USE_SSL */
