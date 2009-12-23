/* Copyright 2008 Uppsala University
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *     
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */ 
#ifndef _BLOOMFILTER_H
#define _BLOOMFILTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Enable counting bloomfilter support */
//#define COUNTING_BLOOMFILTER
#ifdef _WIN32
#include <windef.h>
#include <winsock2.h>
#ifndef HAS_DEFINED_U_INTN_T
#define HAS_DEFINED_U_INTN_T
typedef	BYTE u_int8_t;
typedef USHORT u_int16_t;
typedef DWORD32 u_int32_t;
#endif
#else
#include <sys/types.h>
#include <netinet/in.h>
#endif


typedef u_int32_t salt_t;

struct bloomfilter {
	u_int32_t k; /* Number of salts = # hash functions. 32-bits for this is probably overkill, 
		     but it keeps word alignment in the struct so that it is easier to serialize and send over the net */
	u_int32_t m; /* Number of bins/bits in filter */
	u_int32_t n; /* Number of inserted objects */
	/* Here follows k salts */
	/* Then follows the actual filter */
};


/* Bloomfilter bin size in bits */
#ifdef COUNTING_BLOOMFILTER
#define BIN_BITS (sizeof(u_int16_t)*8)
#else
#define BIN_BITS (1)
#endif

#define K_SIZE sizeof(u_int32_t)
#define M_SIZE sizeof(u_int32_t)
#define N_SIZE sizeof(u_int32_t)
#define SALT_SIZE sizeof(salt_t)

#define FILTER_LEN(bf) ((bf)->m*BIN_BITS/8)
#define SALTS_LEN(bf) ((bf)->k*SALT_SIZE)
#define BLOOMFILTER_TOT_LEN(bf) (sizeof(struct bloomfilter) + SALTS_LEN(bf) + FILTER_LEN(bf))

#define BLOOMFILTER_GET_SALTS(bf) ((char *)((char *)(bf) + sizeof(struct bloomfilter)))
#define BLOOMFILTER_GET_FILTER(bf) ((char *)((char *)(bf) + sizeof(struct bloomfilter) + SALTS_LEN(bf)))

enum bf_op {
	bf_op_check,
#define BF_OP_CHECK   bf_op_check
	bf_op_add,
#define BF_OP_ADD     bf_op_add
#ifdef COUNTING_BLOOMFILTER
	bf_op_remove,
#define BF_OP_REMOVE  bf_op_remove
#endif /* COUNTING_BLOOMFILTER */
	BF_OP_MAX,
};

#if defined(WIN32) || defined(WINCE)
#if !defined(inline)
#define inline __inline
#endif
#endif

struct bloomfilter *bloomfilter_new(float error_rate, unsigned int capacity);
int bloomfilter_operation(struct bloomfilter *bf, const char *key, const unsigned int len, unsigned int op);
void bloomfilter_free(struct bloomfilter *bf);
struct bloomfilter *bloomfilter_copy(const struct bloomfilter *bf);

char *bloomfilter_to_base64(const struct bloomfilter *bf);
struct bloomfilter *base64_to_bloomfilter(const char *b64str, const size_t b64len);

static inline unsigned long bloomfilter_get_n(const struct bloomfilter *bf)
{
	return (unsigned long)bf->n;
}

static inline int bloomfilter_check(struct bloomfilter *bf, const char *key, const unsigned int len)
{
	return bloomfilter_operation(bf, key, len, BF_OP_CHECK);
}
static inline int bloomfilter_add(struct bloomfilter *bf, const char *key, const unsigned int len)
{
	return bloomfilter_operation(bf, key, len, BF_OP_ADD);
}
#ifdef DEBUG
void bloomfilter_print(struct bloomfilter *bf);
#endif

#ifdef COUNTING_BLOOMFILTER
static inline int bloomfilter_remove(struct bloomfilter *bf, const char *key, const unsigned int len)
{
	return bloomfilter_operation(bf, key, len, BF_OP_REMOVE);
}
#endif

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

#endif /*_BLOOMFILTER_H */
