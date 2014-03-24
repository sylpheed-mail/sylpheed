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

#ifndef __SSL_HOSTNAME_VALIDATION_H__
#define __SSL_HOSTNAME_VALIDATION_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_SSL

#include <openssl/x509.h>
 
typedef enum {
	SSL_HOSTNAME_MATCH_FOUND,
	SSL_HOSTNAME_MATCH_NOT_FOUND,
	SSL_HOSTNAME_NO_SAN_PRESENT,
	SSL_HOSTNAME_MALFORMED_CERTIFICATE,
	SSL_HOSTNAME_ERROR
} SSLHostnameValidationResult;

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
SSLHostnameValidationResult ssl_validate_hostname(const char *hostname, const X509 *server_cert);

#endif /* USE_SSL */

#endif /* __SSL_HOSTNAME_VALIDATION_H__ */
