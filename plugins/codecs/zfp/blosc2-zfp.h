/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef BLOSC2_ZFP_H
#define BLOSC2_ZFP_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif

int blosc2_zfp_acc_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                            uint8_t meta, blosc2_cparams *cparams, const void* chunk);

int blosc2_zfp_acc_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                              uint8_t meta, blosc2_dparams *dparams, const void* chunk);

int blosc2_zfp_prec_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                            uint8_t meta, blosc2_cparams *cparams, const void* chunk);

int blosc2_zfp_prec_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                              uint8_t meta, blosc2_dparams *dparams, const void* chunk);

int blosc2_zfp_rate_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                             uint8_t meta, blosc2_cparams *cparams, const void* chunk);

int blosc2_zfp_rate_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                               uint8_t meta, blosc2_dparams *dparams, const void* chunk);

#if defined (__cplusplus)
}
#endif

#endif /* BLOSC2_ZFP_H */
