#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>

static SEXP safe_string(const char *x){
  if(x == NULL)
    return NA_STRING;
  return Rf_mkChar(x);
}

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
    SET_STRING_ELT(type, i, safe_string(av_get_media_type_string(codec->type)));
    SET_STRING_ELT(name, i, safe_string(codec->name));
    SET_STRING_ELT(desc, i, safe_string(codec->long_name));
    LOGICAL(enc)[i] = av_codec_is_encoder(codec);
    i++;
  }
  SEXP out = Rf_list4(type, enc, name, desc);
  UNPROTECT(4);
  return out;
}
