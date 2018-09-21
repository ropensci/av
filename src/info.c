#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

static SEXP safe_string(const char *x){
  if(x == NULL)
    return NA_STRING;
  return Rf_mkString(x);
}

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static SEXP get_video_info(AVFormatContext *demuxer){
  SEXP names = PROTECT(Rf_allocVector(STRSXP, 6));
  SET_STRING_ELT(names, 0, Rf_mkChar("width"));
  SET_STRING_ELT(names, 1, Rf_mkChar("height"));
  SET_STRING_ELT(names, 2, Rf_mkChar("codec"));
  SET_STRING_ELT(names, 3, Rf_mkChar("frames"));
  SET_STRING_ELT(names, 4, Rf_mkChar("framerate"));
  SET_STRING_ELT(names, 5, Rf_mkChar("format"));
  for (int i = 0; i < demuxer->nb_streams; i++) {
    AVStream *stream = demuxer->streams[i];
    if(stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
      continue;
    AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if(!codec)
      Rf_error("Failed to find codec");
    AVRational framerate = av_guess_frame_rate(demuxer, stream, NULL);
    SEXP streamdata = PROTECT(Rf_allocVector(VECSXP, Rf_length(names)));
    SET_VECTOR_ELT(streamdata, 0, Rf_ScalarReal(stream->codecpar->width));
    SET_VECTOR_ELT(streamdata, 1, Rf_ScalarReal(stream->codecpar->height));
    SET_VECTOR_ELT(streamdata, 2, safe_string(codec->name));
    SET_VECTOR_ELT(streamdata, 3, Rf_ScalarReal(stream->nb_frames ? stream->nb_frames : NA_REAL));
    SET_VECTOR_ELT(streamdata, 4, Rf_ScalarReal((double)framerate.num / framerate.den));
    SET_VECTOR_ELT(streamdata, 5, safe_string(av_get_pix_fmt_name((enum AVPixelFormat) stream->codecpar->format)));
    Rf_setAttrib(streamdata, R_NamesSymbol, names);
    UNPROTECT(2);
    return streamdata;
  }
  UNPROTECT(1);
  return R_NilValue;
}

static SEXP get_audio_info(AVFormatContext *demuxer){
  SEXP names = PROTECT(Rf_allocVector(STRSXP, 6));
  SET_STRING_ELT(names, 0, Rf_mkChar("channels"));
  SET_STRING_ELT(names, 1, Rf_mkChar("sample_rate"));
  SET_STRING_ELT(names, 2, Rf_mkChar("codec"));
  SET_STRING_ELT(names, 3, Rf_mkChar("frames"));
  SET_STRING_ELT(names, 4, Rf_mkChar("bitrate"));
  SET_STRING_ELT(names, 5, Rf_mkChar("layout"));
  for (int i = 0; i < demuxer->nb_streams; i++) {
    AVStream *stream = demuxer->streams[i];
    if(stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
      continue;
    AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if(!codec)
      Rf_error("Failed to find codec");
    SEXP streamdata = PROTECT(Rf_allocVector(VECSXP, Rf_length(names)));
    SET_VECTOR_ELT(streamdata, 0, Rf_ScalarReal(stream->codecpar->channels));
    SET_VECTOR_ELT(streamdata, 1, Rf_ScalarReal(stream->codecpar->sample_rate));
    SET_VECTOR_ELT(streamdata, 2, Rf_mkString(codec->name));
    SET_VECTOR_ELT(streamdata, 3, Rf_ScalarReal(stream->nb_frames ? stream->nb_frames : NA_REAL));
    SET_VECTOR_ELT(streamdata, 4, Rf_ScalarReal(stream->codecpar->bit_rate));

    char layout[1024] = "";
    av_get_channel_layout_string(layout, 1024, stream->codecpar->channels, stream->codecpar->channel_layout);
    SET_VECTOR_ELT(streamdata, 5, safe_string(layout));
    Rf_setAttrib(streamdata, R_NamesSymbol, names);
    UNPROTECT(2);
    return streamdata;
  }
  UNPROTECT(1);
  return R_NilValue;
}

SEXP R_video_info(SEXP file){
  AVFormatContext *demuxer = NULL;
  bail_if(avformat_open_input(&demuxer, CHAR(STRING_ELT(file, 0)), NULL, NULL), "avformat_open_input");
  bail_if(avformat_find_stream_info(demuxer, NULL), "avformat_find_stream_info");
  SEXP out = PROTECT(Rf_allocVector(VECSXP, 3));
  SEXP outnames = PROTECT(Rf_allocVector(STRSXP, 3));
  SET_STRING_ELT(outnames, 0, Rf_mkChar("duration"));
  SET_STRING_ELT(outnames, 1, Rf_mkChar("video"));
  SET_STRING_ELT(outnames, 2, Rf_mkChar("audio"));
  SET_VECTOR_ELT(out, 0, Rf_ScalarReal(demuxer->duration / 1e6));
  SET_VECTOR_ELT(out, 1, get_video_info(demuxer));
  SET_VECTOR_ELT(out, 2, get_audio_info(demuxer));
  Rf_setAttrib(out, R_NamesSymbol, outnames);
  UNPROTECT(2);
  avformat_close_input(&demuxer);
  avformat_free_context(demuxer);
  return out;
}
