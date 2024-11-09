#include <Rinternals.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/version.h>
#include <libavfilter/avfilter.h>
#include <libavutil/pixdesc.h>
#include "avcompat.h"

enum AVPixelFormat get_default_pix_fmt(const AVCodec *codec){
#ifdef NEW_DEFAULT_CODEC_API
  const enum AVPixelFormat *p = NULL;
  avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void **) &p, NULL);
#else
  const enum AVPixelFormat *p = codec->pix_fmts;
#endif
  return p ? p[0] : AV_PIX_FMT_NONE;
}

enum AVSampleFormat get_default_sample_fmt(const AVCodec *codec){
#ifdef NEW_DEFAULT_CODEC_API
  const enum AVSampleFormat *p = NULL;
  avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **) &p, NULL);
#else
  const enum AVSampleFormat *p = codec->sample_fmts;
#endif
  return p ? p[0] :AV_SAMPLE_FMT_NONE;
}

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

SEXP R_list_codecs(void){
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
    if(codec->type == AVMEDIA_TYPE_AUDIO){
      SET_STRING_ELT(fmt, i, safe_string(av_get_sample_fmt_name(get_default_sample_fmt(codec))));
    } else if(codec->type == AVMEDIA_TYPE_VIDEO){
      SET_STRING_ELT(fmt, i, safe_string(av_get_pix_fmt_name(get_default_pix_fmt(codec))));
    }
    i++;
  }
  SEXP out = Rf_list5(type, enc, name, desc, fmt);
  UNPROTECT(5);
  return out;
}

SEXP R_list_filters(void){
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

SEXP R_list_muxers(void){
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
    const AVCodec *audio_codec = avcodec_find_encoder(muxer->audio_codec);
    const AVCodec *video_codec = avcodec_find_encoder(muxer->video_codec);
    SET_STRING_ELT(audio, i, safe_string(audio_codec ? audio_codec->name : NULL));
    SET_STRING_ELT(video, i, safe_string(video_codec ? video_codec->name : NULL));
    i++;
  }
  SEXP out = Rf_list6(name, mime, extensions, audio, video, desc);
  UNPROTECT(6);
  return out;
}

SEXP R_list_demuxers(void){
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
