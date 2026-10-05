#ifndef F264DEC_H_
#define F264DEC_H_

/**
 * \file f264dec.h
 * \brief Public API of the f264dec H.264/AVC decoder library (normalized with Kvazaar conventions).
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(F264_DLL_EXPORTS)
  #if defined(_WIN32) || defined(__CYGWIN__)
    #define F264_PUBLIC __declspec(dllexport)
  #elif defined(__GNUC__)
    #define F264_PUBLIC __attribute__ ((visibility ("default")))
  #else
    #define F264_PUBLIC
  #endif
#else
  #define F264_PUBLIC
#endif

#define F264_VERSION_MAJOR 1
#define F264_VERSION_MINOR 0
#define F264_VERSION_REV   0

typedef uint8_t f264_pixel;

/**
 * \brief Chroma subsampling formats.
 */
typedef enum f264_chroma_format {
  F264_CSP_400 = 0,
  F264_CSP_420 = 1,
  F264_CSP_422 = 2,
  F264_CSP_444 = 3,
} f264_chroma_format;

/**
 * \brief Return / status codes.
 */
typedef enum f264_result {
  F264_OK        = 0,
  F264_EOS       = 1,
  F264_ERR       = -1,
  F264_ERR_PARAM = -2,
  F264_ERR_MEM   = -3,
} f264_result;

/**
 * \brief Structure representing decoded frame picture data.
 */
typedef struct f264_picture {
  f264_pixel *fulldata_buf;   //!< Allocated buffer with padding (if owned)
  f264_pixel *fulldata;       //!< Allocated buffer portion actually used

  f264_pixel *y;              //!< Pointer to luma plane
  f264_pixel *u;              //!< Pointer to chroma U plane
  f264_pixel *v;              //!< Pointer to chroma V plane
  f264_pixel *data[3];        //!< Array access: [0]=Y, [1]=U, [2]=V

  int32_t width;              //!< Luma width in pixels
  int32_t height;             //!< Luma height in pixels
  int32_t stride;             //!< Luma stride in bytes

  int32_t width_c;            //!< Chroma width in pixels
  int32_t height_c;           //!< Chroma height in pixels
  int32_t stride_c;           //!< Chroma stride in bytes

  int32_t bit_depth;          //!< Bit depth (e.g. 8, 10)
  int64_t pts;                //!< Presentation timestamp
  int64_t dts;                //!< Decompression timestamp
  f264_chroma_format chroma_format; //!< Chroma subsampling format
  int32_t poc;                //!< Picture Order Count

  void *priv;                 //!< Internal decoder reference
} f264_picture;

typedef f264_picture f264dec_picture_t;

typedef void (*f264_error_callback)(void *user_data, int code, const char *msg);

/**
 * \brief Decoder configuration settings.
 */
typedef struct f264_config {
  char infile[512];           //!< Input bitstream file path
  char outfile[512];          //!< Output YUV file path (empty = no disk write)
  char reffile[512];          //!< Reference YUV for SNR calculation
  int32_t threads;            //!< Number of worker threads (0 = auto, 1 = single-threaded)
  int32_t max_frames;         //!< Maximum frames to decode (0 = all)
  int32_t cpuid;              //!< Enable/disable SIMD optimizations (1 = SIMD, 0 = Generic)
  int32_t silent;             //!< Suppress console output (1 = silent, 0 = verbose)
  int32_t postproc_level;     //!< Postprocessing level [0..100]
  int32_t deblock_enable;     //!< Enable deblocking filter (default: 1)
  int32_t file_format;        //!< Input bitstream format (0 = Annex B)
  int32_t memory_input;       //!< In-memory input flag (1 = stream via f264_decoder_push, 0 = file)
  f264_error_callback error_cb; //!< Optional error callback (NULL = print to stderr)
  void *error_cb_user_data;     //!< User context pointer passed to error_cb
} f264_config;

typedef f264_config f264dec_config_t;

/**
 * \brief Opaque decoder instance handle.
 * \note Currently f264dec supports one active decoder instance per process.
 */
typedef struct f264_decoder f264_decoder;
typedef f264_decoder f264dec_t;

/**
 * \brief f264dec function table API (normalized with Kvazaar kvz_api).
 */
typedef struct f264_api {
  /**
   * \brief Allocate a f264_config structure.
   */
  f264_config * (*config_alloc)(void);

  /**
   * \brief Deallocate a f264_config structure.
   */
  void          (*config_destroy)(f264_config *cfg);

  /**
   * \brief Initialize config structure to default settings.
   */
  int           (*config_init)(f264_config *cfg);

  /**
   * \brief Set a configuration parameter by key-value string.
   */
  int           (*config_parse)(f264_config *cfg, const char *name, const char *value);

  /**
   * \brief Open and initialize a decoder instance.
   * \note Only one active decoder instance can exist per process. Returns NULL if
   *       an instance is already active.
   */
  f264_decoder *(*decoder_open)(const f264_config *cfg);

  /**
   * \brief Close and release a decoder instance.
   */
  void          (*decoder_close)(f264_decoder *dec);

  /**
   * \brief Decode next frame from bitstream.
   * \param dec      Decoder instance
   * \param pic_out  Pointer to store decoded picture pointer (or NULL if no picture ready).
   *                 Plane pointers (y, u, v) alias internal decoder memory valid until
   *                 the next call to decoder_decode, decoder_get_picture, or decoder_flush.
   *                 Copy pixels if needed across iterations.
   * \return F264_OK on success, F264_EOS at end of stream, or negative error code.
   * \note When using memory_input, one access unit of lookahead is required to detect
   *       picture boundaries (the decoder must see the start of the next picture before
   *       emitting the current one).
   */
  int           (*decoder_decode)(f264_decoder *dec, f264_picture **pic_out);

  /**
   * \brief Flush remaining queued pictures from the DPB.
   * \note Call repeatedly until it returns F264_EOS to drain all remaining pictures.
   */
  int           (*decoder_flush)(f264_decoder *dec, f264_picture **pic_out);

  /**
   * \brief Push Annex B bitstream chunk into in-memory decoder buffer.
   * \param dec  Decoder instance
   * \param data Pointer to bitstream bytes
   * \param size Number of bytes
   * \return Bytes pushed on success, or negative error code.
   */
  int           (*decoder_push)(f264_decoder *dec, const uint8_t *data, size_t size);

  /**
   * \brief Consume and retrieve next available decoded picture from output list.
   * \param dec Decoder instance
   * \return Pointer to next valid f264_picture (aliased), or NULL if none available.
   */
  f264_picture *(*decoder_get_picture)(f264_decoder *dec);

  /**
   * \brief Allocate an empty picture container with allocated buffers.
   */
  f264_picture *(*picture_alloc)(int32_t width, int32_t height);

  /**
   * \brief Allocate picture with specific chroma subsampling format.
   */
  f264_picture *(*picture_alloc_csp)(f264_chroma_format csp, int32_t width, int32_t height);

  /**
   * \brief Deallocate a picture allocated via picture_alloc.
   */
  void          (*picture_free)(f264_picture *pic);
} f264_api;

typedef f264_api f264dec_api_t;

/* --- Core API accessors --- */

/**
 * \brief Retrieve the f264dec API table (matches kvz_api_get).
 * \param bit_depth Internal bit depth (typically 8)
 * \return Pointer to const f264_api struct
 */
F264_PUBLIC const f264_api * f264_api_get(int bit_depth);
F264_PUBLIC const f264_api * f264dec_api_get(int bit_depth);

/* --- Standalone functions prefixed with f264_ --- */

F264_PUBLIC f264_config *    f264_config_alloc(void);
F264_PUBLIC void             f264_config_destroy(f264_config *cfg);
F264_PUBLIC int              f264_config_init(f264_config *cfg);
F264_PUBLIC int              f264_config_parse(f264_config *cfg, const char *name, const char *value);

F264_PUBLIC f264_decoder *   f264_decoder_open(const f264_config *cfg);
F264_PUBLIC void             f264_decoder_close(f264_decoder *dec);
F264_PUBLIC int              f264_decoder_decode(f264_decoder *dec, f264_picture **pic_out);
F264_PUBLIC int              f264_decoder_flush(f264_decoder *dec, f264_picture **pic_out);
F264_PUBLIC int              f264_decoder_push(f264_decoder *dec, const uint8_t *data, size_t size);
F264_PUBLIC f264_picture *   f264_decoder_get_picture(f264_decoder *dec);

F264_PUBLIC f264_picture *   f264_picture_alloc(int32_t width, int32_t height);
F264_PUBLIC f264_picture *   f264_picture_alloc_csp(f264_chroma_format csp, int32_t width, int32_t height);
F264_PUBLIC void             f264_picture_free(f264_picture *pic);

F264_PUBLIC const char *     f264_get_version_string(void);
F264_PUBLIC int              f264_get_version_major(void);
F264_PUBLIC int              f264_get_version_minor(void);
F264_PUBLIC int              f264_get_version_revision(void);

/* --- Aliases prefixed with f264dec_ --- */

#define f264dec_config_alloc       f264_config_alloc
#define f264dec_config_destroy     f264_config_destroy
#define f264dec_config_init        f264_config_init
#define f264dec_config_parse       f264_config_parse
#define f264dec_open               f264_decoder_open
#define f264dec_close              f264_decoder_close
#define f264dec_decode_frame       f264_decoder_decode
#define f264dec_flush              f264_decoder_flush
#define f264dec_push               f264_decoder_push
#define f264dec_get_picture        f264_decoder_get_picture
#define f264dec_picture_alloc      f264_picture_alloc
#define f264dec_picture_alloc_csp  f264_picture_alloc_csp
#define f264dec_picture_free       f264_picture_free
#define f264dec_get_version_string f264_get_version_string

#ifdef __cplusplus
}
#endif

#endif // F264DEC_H_
