#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libavfilter/avfilter.h>

static SEXP safe_string(const char *x){
  if(x == NULL)
    return NA_STRING;
  return Rf_mkChar(x);
}

#if LIBAVFORMAT_VERSION_MAJOR < 58 // FFmpeg 4.0
#define iterate_over_codec(x) (x = av_codec_next(x))
#define iterate_over_filter(x) (x = av_filter_next(x))
#else
#define iterate_over_codec(x) (x = av_codec_iterate(&iter))
#define iterate_over_filter(x) (x = av_filter_iterate(&iter))
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

SEXP R_list_filters(){
  const AVFilter *filter = NULL;
  void * iter = NULL;
  int count = 0;
  while(iterate_over_filter(filter))
    count++;
  SEXP name = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP desc = PROTECT(Rf_allocVector(STRSXP, count));

  // Re-iterate from scratch
  iter = NULL;
  int i = 0;
  while(iterate_over_filter(filter)) {
    SET_STRING_ELT(name, i, safe_string(filter->name));
    SET_STRING_ELT(desc, i, safe_string(filter->description));
    i++;
  }
  SEXP out = Rf_list2(name, desc);
  UNPROTECT(2);
  return out;
}
