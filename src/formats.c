#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>

#if LIBAVFORMAT_VERSION_MAJOR < 58 // FFmpeg 4.0
#define iterate_over_codec(x) (x = av_codec_next(x))
#else
#define iterate_over_codec(x) (x = av_codec_iterate(&iter))
#endif

SEXP R_list_codecs(){
  const AVCodec *codec = NULL;
  void * iter = NULL;
  int count = 0;
  while(iterate_over_codec(codec))
    count++;
  SEXP type = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP name = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP desc = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP enc = PROTECT(Rf_allocVector(LGLSXP, count));
  
  // Re-iterate from scratch
  iter = NULL;
  int i = 0;
  while(iterate_over_codec(codec)) {
    SET_STRING_ELT(type, i, Rf_mkChar(av_get_media_type_string(codec->type)));
    SET_STRING_ELT(name, i, Rf_mkChar(codec->name));
    SET_STRING_ELT(desc, i, Rf_mkChar(codec->long_name));
    LOGICAL(enc)[i] = av_codec_is_encoder(codec);
    i++;
  }
  SEXP out = Rf_list4(type, enc, name, desc);
  UNPROTECT(4);
  return out;
}
