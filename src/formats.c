#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

SEXP R_list_codecs(){
  const AVCodec *codec = NULL;
  void * iter = NULL;
  int count = 0;
  while ((codec = av_codec_iterate(&iter)))
    count++;
  SEXP type = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP name = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP desc = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP enc = PROTECT(Rf_allocVector(LGLSXP, count));
  
  // Re-iterate from scratch
  iter = NULL;
  int i = 0;
  while ((codec = av_codec_iterate(&iter))) {
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
