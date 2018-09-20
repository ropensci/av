#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libavfilter/avfilter.h>
#include <libavutil/pixdesc.h>

static SEXP safe_string(const char *x){
  if(x == NULL)
    return NA_STRING;
  return Rf_mkChar(x);
}

#if LIBAVFORMAT_VERSION_MAJOR < 58 // FFmpeg 4.0
#define iterate_over_codec(x) (x = av_codec_next(x))
#define iterate_over_filter(x) (x = avfilter_next(x))
#define iterate_over_muxer(x) (x = av_oformat_next(x))
#define iterate_over_demuxer(x) (x = av_iformat_next(x))
#else
#define iterate_over_codec(x) (x = av_codec_iterate(&iter))
#define iterate_over_filter(x) (x = av_filter_iterate(&iter))
#define iterate_over_muxer(x) (x = av_muxer_iterate(&iter))
#define iterate_over_demuxer(x) (x = av_demuxer_iterate(&iter))
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
  SEXP fmt = PROTECT(Rf_allocVector(STRSXP, count));

  // Re-iterate from scratch
  iter = NULL;
  int i = 0;
  while(iterate_over_codec(codec)) {
    SET_STRING_ELT(type, i, safe_string(av_get_media_type_string(codec->type)));
    SET_STRING_ELT(name, i, safe_string(codec->name));
    SET_STRING_ELT(desc, i, safe_string(codec->long_name));
    LOGICAL(enc)[i] = av_codec_is_encoder(codec);
    if(codec->type == AVMEDIA_TYPE_AUDIO && codec->sample_fmts){
      SET_STRING_ELT(fmt, i, safe_string(av_get_sample_fmt_name(codec->sample_fmts[0])));
    } else if(codec->type == AVMEDIA_TYPE_VIDEO && codec->pix_fmts){
      SET_STRING_ELT(fmt, i, safe_string(av_get_pix_fmt_name(codec->pix_fmts[0])));
    }
    i++;
  }
  SEXP out = Rf_list5(type, enc, name, desc, fmt);
  UNPROTECT(5);
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

SEXP R_list_muxers(){
  const AVOutputFormat *muxer = NULL;
  void * iter = NULL;
  int count = 0;
  while(iterate_over_muxer(muxer))
    count++;
  SEXP name = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP desc = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP mime = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP extensions = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP audio = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP video = PROTECT(Rf_allocVector(STRSXP, count));

  // Re-iterate from scratch
  iter = NULL;
  int i = 0;
  while(iterate_over_muxer(muxer)) {
    SET_STRING_ELT(name, i, safe_string(muxer->name));
    SET_STRING_ELT(desc, i, safe_string(muxer->long_name));
    SET_STRING_ELT(mime, i, safe_string(muxer->mime_type));
    SET_STRING_ELT(extensions, i, safe_string(muxer->extensions));
    AVCodec *audio_codec = avcodec_find_encoder(muxer->audio_codec);
    AVCodec *video_codec = avcodec_find_encoder(muxer->video_codec);
    SET_STRING_ELT(audio, i, safe_string(audio_codec ? audio_codec->name : NULL));
    SET_STRING_ELT(video, i, safe_string(video_codec ? video_codec->name : NULL));
    i++;
  }
  SEXP out = Rf_list6(name, mime, extensions, audio, video, desc);
  UNPROTECT(6);
  return out;
}

SEXP R_list_demuxers(){
  const AVInputFormat *muxer = NULL;
  void * iter = NULL;
  int count = 0;
  while(iterate_over_demuxer(muxer))
    count++;
  SEXP name = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP desc = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP mime = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP extensions = PROTECT(Rf_allocVector(STRSXP, count));

  // Re-iterate from scratch
  iter = NULL;
  int i = 0;
  while(iterate_over_demuxer(muxer)) {
    SET_STRING_ELT(name, i, safe_string(muxer->name));
    SET_STRING_ELT(desc, i, safe_string(muxer->long_name));
    SET_STRING_ELT(mime, i, safe_string(muxer->mime_type));
    SET_STRING_ELT(extensions, i, safe_string(muxer->extensions));
    i++;
  }
  SEXP out = Rf_list4(name, mime, extensions, desc);
  UNPROTECT(4);
  return out;
}
