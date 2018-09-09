#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

SEXP R_video_info(SEXP file){
  AVFormatContext *ifmt_ctx = NULL;
  bail_if(avformat_open_input(&ifmt_ctx, CHAR(STRING_ELT(file, 0)), NULL, NULL), "avformat_open_input");
  bail_if(avformat_find_stream_info(ifmt_ctx, NULL), "avformat_find_stream_info");
  SEXP duration = PROTECT(Rf_ScalarReal(ifmt_ctx->duration / 1e6));
  SEXP stream_list = PROTECT(Rf_allocVector(VECSXP, ifmt_ctx->nb_streams));
  SEXP streamnames = PROTECT(Rf_allocVector(STRSXP, 5));
  SET_STRING_ELT(streamnames, 0, Rf_mkChar("width"));
  SET_STRING_ELT(streamnames, 1, Rf_mkChar("height"));
  SET_STRING_ELT(streamnames, 2, Rf_mkChar("codec"));
  SET_STRING_ELT(streamnames, 3, Rf_mkChar("frames"));
  SET_STRING_ELT(streamnames, 4, Rf_mkChar("framerate"));
  for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
    SEXP streamdata = PROTECT(Rf_allocVector(VECSXP, 5));
    AVCodec *codec = avcodec_find_decoder(ifmt_ctx->streams[i]->codecpar->codec_id);
    SET_VECTOR_ELT(streamdata, 0, Rf_ScalarReal(ifmt_ctx->streams[i]->codecpar->width));
    SET_VECTOR_ELT(streamdata, 1, Rf_ScalarReal(ifmt_ctx->streams[i]->codecpar->height));
    SET_VECTOR_ELT(streamdata, 2, Rf_mkString(codec->name));
    AVRational framerate = av_guess_frame_rate(ifmt_ctx, ifmt_ctx->streams[i], NULL);
    SET_VECTOR_ELT(streamdata, 3, Rf_ScalarReal(ifmt_ctx->streams[i]->nb_frames));
    SET_VECTOR_ELT(streamdata, 4, Rf_ScalarReal((double)framerate.num / framerate.den));
    SET_VECTOR_ELT(stream_list, i, streamdata);
    Rf_setAttrib(streamdata, R_NamesSymbol, streamnames);
    UNPROTECT(1);
  }
  SEXP out = PROTECT(Rf_allocVector(VECSXP, 2));
  SEXP outnames = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(outnames, 0, Rf_mkChar("duration"));
  SET_STRING_ELT(outnames, 1, Rf_mkChar("video"));
  SET_VECTOR_ELT(out, 0, duration);
  SET_VECTOR_ELT(out, 1, stream_list);
  Rf_setAttrib(out, R_NamesSymbol, outnames);
  UNPROTECT(5);
  avformat_close_input(&ifmt_ctx);
  avformat_free_context(ifmt_ctx);
  return out;
}
