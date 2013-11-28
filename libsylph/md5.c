/*

  GNet API added by David Helder <dhelder@umich.edu> 2000-6-11.  All
  additions and changes placed in the public domain.

  Files originally from: http://www.gxsnmp.org/CVS/gxsnmp/

  Modified the prefix of functions to prevent conflict with original GNet.

 */
/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#include "md5.h"
#include <glib.h>
#include <string.h>


/* ************************************************************ */
/* Code below is from Colin Plumb implementation 		*/



struct MD5Context {
	guint32 buf[4];
	guint32 bits[2];
	guchar  in[64];
	int     doByteReverse;
};

static void MD5Init(struct MD5Context *context);
static void MD5Update(struct MD5Context *context, guchar const *buf,
		      guint len);
static void MD5Final(guchar digest[16], struct MD5Context *context);
static void MD5Transform(guint32 buf[4], guint32 const in[16]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;




static void byteReverse(guint8 *buf, guint longs);

/*
 * Note: this code is harmless on little-endian machines.
 */
void 
byteReverse(guint8 *buf, guint longs)
{
  guint32 t;
  do 
    {
      t = (guint32) ((guint) buf[3] << 8 | buf[2]) << 16 |
          ((guint) buf[1] << 8 | buf[0]);
      *(guint32 *) buf = t;
      buf += 4;
    } 
  while (--longs);
}

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void 
MD5Init(struct MD5Context *ctx)
{
  ctx->buf[0] = 0x67452301;
  ctx->buf[1] = 0xefcdab89;
  ctx->buf[2] = 0x98badcfe;
  ctx->buf[3] = 0x10325476;

  ctx->bits[0] = 0;
  ctx->bits[1] = 0;

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
  ctx->doByteReverse = 1;
#else
  ctx->doByteReverse = 0;
#endif
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void 
MD5Update(struct MD5Context *ctx, guint8 const *buf, guint len)
{
  guint32 t;

  /* Update bitcount */

  t = ctx->bits[0];
  if ((ctx->bits[0] = t + ((guint32) len << 3)) < t)
    ctx->bits[1]++;		/* Carry from low to high */
  ctx->bits[1] += len >> 29;

  t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

  /* Handle any leading odd-sized chunks */

  if (t) 
    {
      guint8 *p = (guint8 *) ctx->in + t;

      t = 64 - t;
      if (len < t) 
        {
          g_memmove(p, buf, len);
          return;
	}
      g_memmove(p, buf, t);
      if (ctx->doByteReverse)
        byteReverse(ctx->in, 16);
      MD5Transform(ctx->buf, (guint32 *) ctx->in);
      buf += t;
      len -= t;
    }
  /* Process data in 64-byte chunks */

  while (len >= 64) 
    {
      g_memmove(ctx->in, buf, 64);
      if (ctx->doByteReverse)
        byteReverse(ctx->in, 16);
      MD5Transform(ctx->buf, (guint32 *) ctx->in);
      buf += 64;
      len -= 64;
    }

  /* Handle any remaining bytes of data. */

  g_memmove(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void 
MD5Final(guint8 digest[16], struct MD5Context *ctx)
{
  guint count;
  guint8 *p;

  /* Compute number of bytes mod 64 */
  count = (ctx->bits[0] >> 3) & 0x3F;

  /* Set the first char of padding to 0x80.  This is safe since there is
     always at least one byte free */
  p = ctx->in + count;
  *p++ = 0x80;

  /* Bytes of padding needed to make 64 bytes */
  count = 64 - 1 - count;

  /* Pad out to 56 mod 64 */
  if (count < 8) 
    {
	/* Two lots of padding:  Pad the first block to 64 bytes */
      memset(p, 0, count);
      if (ctx->doByteReverse)
        byteReverse(ctx->in, 16);
      MD5Transform(ctx->buf, (guint32 *) ctx->in);

      /* Now fill the next block with 56 bytes */
      memset(ctx->in, 0, 56);
    } 
  else 
    {
      /* Pad block to 56 bytes */
      memset(p, 0, count - 8);
    }
  if (ctx->doByteReverse)
    byteReverse(ctx->in, 14);

  /* Append length in bits and transform */
  ((guint32 *) ctx->in)[14] = ctx->bits[0];
  ((guint32 *) ctx->in)[15] = ctx->bits[1];

  MD5Transform(ctx->buf, (guint32 *) ctx->in);
  if (ctx->doByteReverse)
    byteReverse((guint8 *) ctx->buf, 4);
  g_memmove(digest, ctx->buf, 16);
  memset(ctx, 0, sizeof(*ctx));	/* In case it's sensitive */
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void 
MD5Transform(guint32 buf[4], guint32 const in[16])
{
  register guint32 a, b, c, d;

  a = buf[0];
  b = buf[1];
  c = buf[2];
  d = buf[3];

  MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
  MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
  MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
  MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
  MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
  MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
  MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
  MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
  MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
  MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
  MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
  MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
  MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
  MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
  MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
  MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

  MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
  MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
  MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
  MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
  MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
  MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
  MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
  MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
  MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
  MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
  MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
  MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
  MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
  MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
  MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
  MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

  MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
  MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
  MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
  MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
  MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
  MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
  MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
  MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
  MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
  MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
  MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
  MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
  MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
  MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
  MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
  MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

  MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
  MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
  MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
  MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
  MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
  MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
  MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
  MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
  MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
  MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
  MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
  MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
  MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
  MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
  MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
  MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}



/* ************************************************************ */
/* Code below is David Helder's API for GNet			*/

struct _SMD5
{
  struct MD5Context 	ctx;
  gchar 		digest[S_GNET_MD5_HASH_LENGTH];
};


/**
 *  s_gnet_md5_new:
 *  @buffer: buffer to hash
 *  @length: length of @buffer
 * 
 *  Creates a #SMD5 from @buffer.
 *
 *  Returns: a new #SMD5.
 *
 **/
SMD5*           
s_gnet_md5_new (const guchar* buffer, guint length)
{
  SMD5* md5;

  md5 = g_new0 (SMD5, 1);
  MD5Init (&md5->ctx);
  MD5Update (&md5->ctx, buffer, length);
  MD5Final ((gpointer) &md5->digest, &md5->ctx);

  return md5;
}



/**
 *  s_gnet_md5_new_string:
 *  @str: hexidecimal string
 * 
 *  Creates a #SMD5 from @str.  @str is a hexidecimal string
 *  representing the digest.
 *
 *  Returns: a new #SMD5.
 *
 **/
SMD5*		
s_gnet_md5_new_string (const gchar* str)
{
  SMD5* md5;
  guint i;

  g_return_val_if_fail (str, NULL);
  g_return_val_if_fail (strlen(str) >= (S_GNET_MD5_HASH_LENGTH * 2), NULL);

  md5 = g_new0 (SMD5, 1);

  for (i = 0; i < (S_GNET_MD5_HASH_LENGTH * 2); ++i)
    {
      guint val = 0;

      switch (str[i])
	{
	case '0':	val = 0;	break;
	case '1':	val = 1;	break;
	case '2':	val = 2;	break;
	case '3':	val = 3;	break;
	case '4':	val = 4;	break;
	case '5':	val = 5;	break;
	case '6':	val = 6;	break;
	case '7':	val = 7;	break;
	case '8':	val = 8;	break;
	case '9':	val = 9;	break;
	case 'A':
	case 'a':	val = 10;	break;
	case 'B':
	case 'b':	val = 11;	break;
	case 'C':
	case 'c':	val = 12;	break;
	case 'D':
	case 'd':	val = 13;	break;
	case 'E':
	case 'e':	val = 14;	break;
	case 'F':
	case 'f':	val = 15;	break;
	default:
	  g_return_val_if_fail (FALSE, NULL);
	}

      if (i % 2)
	md5->digest[i / 2] |= val;
      else
	md5->digest[i / 2] = val << 4;
    }

  return md5;
}



/**
 *  s_gnet_md5_clone
 *  @md5: a #SMD5
 * 
 *  Copies a #SMD5.
 *
 *  Returns: a copy of @md5.
 *
 **/
SMD5*           
s_gnet_md5_clone (const SMD5* md5)
{
  SMD5* md52;

  g_return_val_if_fail (md5, NULL);

  md52      = g_new0 (SMD5, 1);
  md52->ctx = md5->ctx;
  memcpy (md52->digest, md5->digest, sizeof(md5->digest));

  return md52;
}



/** 
 *  s_gnet_md5_delete
 *  @md5: a #SMD5
 *
 *  Deletes a #SMD5.
 *
 **/
void
s_gnet_md5_delete (SMD5* md5)
{
  if (md5)
    g_free (md5);
}



/**
 *  s_gnet_md5_new_incremental
 *
 *  Creates a #SMD5 incrementally.  After creating a #SMD5, call
 *  s_gnet_md5_update() one or more times to hash data.  Finally, call
 *  s_gnet_md5_final() to compute the final hash value.
 *
 *  Returns: a new #SMD5.
 *
 **/
SMD5*		
s_gnet_md5_new_incremental (void)
{
  SMD5* md5;

  md5 = g_new0 (SMD5, 1);
  MD5Init (&md5->ctx);
  return md5;
}


/**
 *  s_gnet_md5_update
 *  @md5: a #SMD5
 *  @buffer: buffer to add
 *  @length: length of @buffer
 *
 *  Updates the hash with @buffer.  This may be called several times
 *  on a hash created by s_gnet_md5_new_incremental() before being
 *  finalized by calling s_gnet_md5_final().
 * 
 **/
void
s_gnet_md5_update (SMD5* md5, const guchar* buffer, guint length)
{
  g_return_if_fail (md5);

  MD5Update (&md5->ctx, buffer, length);
}


/**
 *  s_gnet_md5_final
 *  @md5: a #SMD5
 *
 *  Calcuates the final hash value of a #SMD5.  This should only be
 *  called on an #SMD5 created by s_gnet_md5_new_incremental().
 *
 **/
void
s_gnet_md5_final (SMD5* md5)
{
  g_return_if_fail (md5);

  MD5Final ((gpointer) &md5->digest, &md5->ctx);
}


/* **************************************** */

/**
 *  s_gnet_md5_equal
 *  @p1: first #SMD5.
 *  @p2: second #SMD5.
 *
 *  Compares two #SMD5's for equality.
 *
 *  Returns: TRUE if they are equal; FALSE otherwise.
 *
 **/
gint
s_gnet_md5_equal (gconstpointer p1, gconstpointer p2)
{
  SMD5* md5a = (SMD5*) p1;
  SMD5* md5b = (SMD5*) p2;
  guint i;

  for (i = 0; i < S_GNET_MD5_HASH_LENGTH; ++i)
    if (md5a->digest[i] != md5b->digest[i])
      return FALSE;

  return TRUE;
}


/**
 *  s_gnet_md5_hash
 *  @p: a #SMD5
 *
 *  Creates a hash code for a #SMD5 for use with GHashTable.  This
 *  hash value is not the same as the MD5 digest.
 *
 *  Returns: the hash code for @p.
 *
 **/
guint
s_gnet_md5_hash (gconstpointer p)
{
  const SMD5* md5 = (const SMD5*) p;
  const guint* q;

  g_return_val_if_fail (md5, 0);

  q = (const guint*) md5->digest;

  return (q[0] ^ q[1] ^ q[2] ^ q[3]);
}


/**
 *  s_gnet_md5_get_digest
 *  @md5: a #SMD5
 *
 *  Gets the raw MD5 digest.
 *
 *  Returns: a callee-owned buffer containing the MD5 hash digest.
 *  The buffer is %S_GNET_MD5_HASH_LENGTH bytes long.
 *
 **/
gchar*        	
s_gnet_md5_get_digest (const SMD5* md5)
{
  g_return_val_if_fail (md5, NULL);
  
  return (gchar*) md5->digest;
}


static gchar bits2hex[16] = { '0', '1', '2', '3', 
			      '4', '5', '6', '7',
			      '8', '9', 'a', 'b',
			      'c', 'd', 'e', 'f' };

/**
 *  s_gnet_md5_get_string
 *  @md5: a #SMD5
 *
 *  Gets the digest represented a human-readable string.
 *
 *  Returns: a hexadecimal string representing the digest.  The string
 *  is 2 * %S_GNET_MD5_HASH_LENGTH bytes long and NULL terminated.  The
 *  string is caller owned.
 *
 **/
gchar*          
s_gnet_md5_get_string (const SMD5* md5)
{
  gchar* str;
  guint i;

  g_return_val_if_fail (md5, NULL);

  str = g_new (gchar, S_GNET_MD5_HASH_LENGTH * 2 + 1);
  str[S_GNET_MD5_HASH_LENGTH * 2] = '\0';

  for (i = 0; i < S_GNET_MD5_HASH_LENGTH; ++i)
    {
      str[i * 2]       = bits2hex[(md5->digest[i] & 0xF0) >> 4];
      str[(i * 2) + 1] = bits2hex[(md5->digest[i] & 0x0F)     ];
    }

  return str;
}



/**
 * s_gnet_md5_copy_string
 * @md5: a #SMD5
 * @buffer: buffer at least 2 * %S_GNET_MD5_HASH_LENGTH bytes long
 *
 * Copies the digest, represented as a string, into @buffer.  The
 * string is not NULL terminated.
 * 
 **/
void
s_gnet_md5_copy_string (const SMD5* md5, gchar* buffer)
{
  guint i;

  g_return_if_fail (md5);
  g_return_if_fail (buffer);

  for (i = 0; i < S_GNET_MD5_HASH_LENGTH; ++i)
    {
      buffer[i * 2]       = bits2hex[(md5->digest[i] & 0xF0) >> 4];
      buffer[(i * 2) + 1] = bits2hex[(md5->digest[i] & 0x0F)     ];
    }
}
