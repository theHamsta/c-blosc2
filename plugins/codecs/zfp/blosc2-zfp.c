#include "blosc2.h"
#include "zfp.h"
#include "zfp-private.h"
#include "blosc2-zfp.h"
#include <math.h>

int blosc2_zfp_acc_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                            int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

    double tol = (int8_t) meta;
/*
    printf("\n input \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* stream containing the real output buffer */
    zfp_stream *zfp_aux;   /* auxiliar compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    bitstream *stream_aux; /* auxiliar bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */
    double tolerance = pow(10, tol);

    int32_t typesize = cparams->typesize;

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_accuracy(zfp, tolerance);
    stream = stream_open(output, output_len);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) input, type, blockshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) input, type, blockshape[1], blockshape[0]);
            break;
        case 3:
            field = zfp_field_3d((void *) input, type, blockshape[2], blockshape[1], blockshape[0]);
            break;
        case 4:
            field = zfp_field_4d((void *) input, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    int zfp_maxout = (int) zfp_stream_maximum_size(zfp, field);
    zfp_stream_close(zfp);
    stream_close(stream);
    uint8_t *aux_out = malloc(zfp_maxout);
    zfp_aux = zfp_stream_open(NULL);
    zfp_stream_set_accuracy(zfp_aux, tolerance);
    stream_aux = stream_open(aux_out, zfp_maxout);
    zfp_stream_set_bit_stream(zfp_aux, stream_aux);
    zfp_stream_rewind(zfp_aux);

    zfpsize = zfp_compress(zfp_aux, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp_aux);
    stream_close(stream_aux);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Compression failed\n");
        free(aux_out);
        return (int) zfpsize;
    }
    if ((int32_t) zfpsize >= input_len) {
        BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
        free(aux_out);
        return 0;
    }

    memcpy(output, aux_out, zfpsize);
    free(aux_out);

    return (int) zfpsize;
}

int blosc2_zfp_acc_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                              int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    size_t typesize;
    int flags;
    blosc_cbuffer_metainfo(chunk, &typesize, &flags);

    double tol = (int8_t) meta;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */
    double tolerance = pow(10, tol);

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_accuracy(zfp, tolerance);
    stream = stream_open((void*) input, input_len);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) output, type, blockshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) output, type, blockshape[1], blockshape[0]);
            break;
        case 3:
            field = zfp_field_3d((void *) output, type, blockshape[2], blockshape[1], blockshape[0]);
            break;
        case 4:
            field = zfp_field_4d((void *) output, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfpsize = zfp_decompress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
        return (int) zfpsize;
    }

    return (int) output_len;
}

int blosc2_zfp_prec_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                            int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

/*
    printf("\n input \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* stream containing the real output buffer */
    zfp_stream *zfp_aux;   /* auxiliar compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    bitstream *stream_aux; /* auxiliar bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */

    uint prec;
    switch (ndim) {
        case 1:
            prec = meta + 5;
            break;
        case 2:
            prec = meta + 7;
            break;
        case 3:
            prec = meta + 9;
            break;
        case 4:
            prec = meta + 11;
            break;
        default:
            printf("\n ZFP is not available for this ndim \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    if(prec > ZFP_MAX_PREC) {
        BLOSC_TRACE_ERROR("Max precision for this codecs is %d", ZFP_MAX_PREC);
        prec = ZFP_MAX_PREC;
    }

    int32_t typesize = cparams->typesize;
    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_precision(zfp, prec);
    stream = stream_open(output, output_len);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) input, type, blockshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) input, type, blockshape[1], blockshape[0]);
            break;
        case 3:
            field = zfp_field_3d((void *) input, type, blockshape[2], blockshape[1], blockshape[0]);
            break;
        case 4:
            field = zfp_field_4d((void *) input, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    int zfp_maxout = (int) zfp_stream_maximum_size(zfp, field);
    zfp_stream_close(zfp);
    stream_close(stream);
    uint8_t *aux_out = malloc(zfp_maxout);
    zfp_aux = zfp_stream_open(NULL);
    zfp_stream_set_precision(zfp_aux, prec);
    stream_aux = stream_open(aux_out, zfp_maxout);
    zfp_stream_set_bit_stream(zfp_aux, stream_aux);
    zfp_stream_rewind(zfp_aux);

    zfpsize = zfp_compress(zfp_aux, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp_aux);
    stream_close(stream_aux);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Compression failed\n");
        free(aux_out);
        return (int) zfpsize;
    }
    if ((int32_t) zfpsize >= input_len) {
        BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
        free(aux_out);
        return 0;
    }

    memcpy(output, aux_out, zfpsize);
    free(aux_out);

    return (int) zfpsize;
}

int blosc2_zfp_prec_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                              int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    size_t typesize;
    int flags;
    blosc_cbuffer_metainfo(chunk, &typesize, &flags);
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */

    uint prec;
    switch (ndim) {
        case 1:
            prec = meta + 5;
            break;
        case 2:
            prec = meta + 7;
            break;
        case 3:
            prec = meta + 9;
            break;
        case 4:
            prec = meta + 11;
            break;
        default:
            printf("\n ZFP is not available for this ndim \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    if(prec > ZFP_MAX_PREC) {
        BLOSC_TRACE_ERROR("Max precision for this codecs is %d", ZFP_MAX_PREC);
        prec = ZFP_MAX_PREC;
    }

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfp = zfp_stream_open(NULL);
    zfp_stream_set_precision(zfp, prec);
    stream = stream_open((void*) input, input_len);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) output, type, blockshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) output, type, blockshape[1], blockshape[0]);
            break;
        case 3:
            field = zfp_field_3d((void *) output, type, blockshape[2], blockshape[1], blockshape[0]);
            break;
        case 4:
            field = zfp_field_4d((void *) output, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfpsize = zfp_decompress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
        return (int) zfpsize;
    }

    return (int) output_len;
}

int blosc2_zfp_rate_compress(const uint8_t *input, int32_t input_len, uint8_t *output,
                             int32_t output_len, uint8_t meta, blosc2_cparams *cparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(cparams);

    double ratio = (double) meta / 100.0;
/*
    printf("\n input \n");
    for (int i = 0; i < input_len; i++) {
        printf("%u, ", input[i]);
    }
*/
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(cparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* stream containing the real output buffer */
    zfp_stream *zfp_aux;   /* auxiliar compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    bitstream *stream_aux; /* auxiliar bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */

    int32_t typesize = cparams->typesize;

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
    }
    double rate = ratio * typesize * 8;     // convert from output size / input size to output bits per input value
    uint cellsize = 1u << (2 * ndim);
    double min_rate;
    switch (type) {
        case zfp_type_float:
            min_rate = (double) (1 + 8u) / cellsize;
            if (rate < min_rate) {
                BLOSC_TRACE_ERROR("\n ZFP minimum rate for this item type is %f. Compression will be done using this rate \n", min_rate);
            }
            break;
        case zfp_type_double:
            min_rate = (double) (1 + 11u) / cellsize;
            if (rate < min_rate) {
                BLOSC_TRACE_ERROR("\n ZFP minimum rate for this item type is %f. Compression will be done using this rate \n", min_rate);
            }
            break;
        default:
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }
    zfp = zfp_stream_open(NULL);
    stream = stream_open(output, output_len);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) input, type, blockshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) input, type, blockshape[1], blockshape[0]);
            break;
        case 3:
            field = zfp_field_3d((void *) input, type, blockshape[2], blockshape[1], blockshape[0]);
            break;
        case 4:
            field = zfp_field_4d((void *) input, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    int zfp_maxout = (int) zfp_stream_maximum_size(zfp, field);
    zfp_stream_close(zfp);
    stream_close(stream);
    uint8_t *aux_out = malloc(zfp_maxout);
    zfp_aux = zfp_stream_open(NULL);
    stream_aux = stream_open(aux_out, zfp_maxout);
    zfp_stream_set_bit_stream(zfp_aux, stream_aux);
    zfp_stream_rewind(zfp_aux);
    zfp_stream_set_rate(zfp_aux, rate, type, ndim, zfp_false);

    zfpsize = zfp_compress(zfp_aux, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp_aux);
    stream_close(stream_aux);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Compression failed\n");
        free(aux_out);
        return (int) zfpsize;
    }
    if ((int32_t) zfpsize >= input_len) {
        BLOSC_TRACE_ERROR("\n ZFP: Compressed data is bigger than input! \n");
        free(aux_out);
        return 0;
    }

    memcpy(output, aux_out, zfpsize);
    free(aux_out);

    return (int) zfpsize;
}

int blosc2_zfp_rate_decompress(const uint8_t *input, int32_t input_len, uint8_t *output,
                              int32_t output_len, uint8_t meta, blosc2_dparams *dparams, const void* chunk) {
    ZFP_ERROR_NULL(input);
    ZFP_ERROR_NULL(output);
    ZFP_ERROR_NULL(dparams);

    size_t typesize;
    int flags;
    blosc_cbuffer_metainfo(chunk, &typesize, &flags);

    double ratio = (double) meta / 100.0;
    int8_t ndim;
    int64_t *shape = malloc(8 * sizeof(int64_t));
    int32_t *chunkshape = malloc(8 * sizeof(int32_t));
    int32_t *blockshape = malloc(8 * sizeof(int32_t));
    uint8_t *smeta;
    uint32_t smeta_len;
    if (blosc2_meta_get(dparams->schunk, "caterva", &smeta, &smeta_len) < 0) {
        printf("Blosc error");
        free(shape);
        free(chunkshape);
        free(blockshape);
        return -1;
    }
    deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape);
    free(smeta);

    zfp_type type;     /* array scalar type */
    zfp_field *field;  /* array meta data */
    zfp_stream *zfp;   /* compressed stream */
    bitstream *stream; /* bit stream to write to or read from */
    size_t zfpsize;    /* byte size of compressed stream */

    switch (typesize) {
        case sizeof(float):
            type = zfp_type_float;
            break;
        case sizeof(double):
            type = zfp_type_double;
            break;
        default:
            printf("\n ZFP is not available for this typesize \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }
    double rate = ratio * typesize * 8;     // convert from output size / input size to output bits per input value
    zfp = zfp_stream_open(NULL);
    zfp_stream_set_rate(zfp, rate, type, ndim, zfp_false);

    stream = stream_open((void*) input, input_len);
    zfp_stream_set_bit_stream(zfp, stream);
    zfp_stream_rewind(zfp);

    switch (ndim) {
        case 1:
            field = zfp_field_1d((void *) output, type, blockshape[0]);
            break;
        case 2:
            field = zfp_field_2d((void *) output, type, blockshape[1], blockshape[0]);
            break;
        case 3:
            field = zfp_field_3d((void *) output, type, blockshape[2], blockshape[1], blockshape[0]);
            break;
        case 4:
            field = zfp_field_4d((void *) output, type, blockshape[3], blockshape[2], blockshape[1], blockshape[0]);
            break;
        default:
            printf("\n ZFP is not available for this number of dims \n");
            free(shape);
            free(chunkshape);
            free(blockshape);
            return 0;
    }

    zfpsize = zfp_decompress(zfp, field);

    /* clean up */
    zfp_field_free(field);
    zfp_stream_close(zfp);
    stream_close(stream);
    free(shape);
    free(chunkshape);
    free(blockshape);

    if (zfpsize < 0) {
        BLOSC_TRACE_ERROR("\n ZFP: Decompression failed\n");
        return (int) zfpsize;
    }

    return (int) output_len;
}

