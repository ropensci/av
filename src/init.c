#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavformat/version.h>
#include <libavfilter/avfilter.h>

/* In theory this should be thread safe and R is not */
static void my_log_callback(void *ptr, int level, const char *fmt, va_list vargs){
  if(level <= av_log_get_level())
    REvprintf(fmt, vargs);
}

static SEXP R_log_level(SEXP new_level){
  if(Rf_length(new_level))
    av_log_set_level(Rf_asInteger(new_level));
  return Rf_ScalarInteger(av_log_get_level());
}

attribute_visible void R_init_av(DllInfo *dll) {
#if LIBAVFORMAT_VERSION_MAJOR < 58 // FFmpeg 4.0
  av_register_all();
  avcodec_register_all();
  avfilter_register_all();
#endif
  avformat_network_init();
  av_log_set_callback(my_log_callback);

  /* .Call calls */
  extern SEXP R_audio_fft(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
  extern SEXP R_audio_bin(SEXP, SEXP, SEXP, SEXP, SEXP);
  extern SEXP R_convert_audio(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
  extern SEXP R_encode_video(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
  extern SEXP R_generate_window(SEXP, SEXP);
  extern SEXP R_get_open_handles();
  extern SEXP R_list_codecs();
  extern SEXP R_list_demuxers();
  extern SEXP R_list_filters();
  extern SEXP R_list_muxers();
  extern SEXP R_log_level(SEXP);
  extern SEXP R_video_info(SEXP);

  static const R_CallMethodDef CallEntries[] = {
    {"R_audio_fft",        (DL_FUNC) &R_audio_fft,        6},
    {"R_audio_bin",        (DL_FUNC) &R_audio_bin,        5},
    {"R_convert_audio",    (DL_FUNC) &R_convert_audio,    7},
    {"R_encode_video",     (DL_FUNC) &R_encode_video,     6},
    {"R_generate_window",  (DL_FUNC) &R_generate_window,  2},
    {"R_get_open_handles", (DL_FUNC) &R_get_open_handles, 0},
    {"R_list_codecs",      (DL_FUNC) &R_list_codecs,      0},
    {"R_list_demuxers",    (DL_FUNC) &R_list_demuxers,    0},
    {"R_list_filters",     (DL_FUNC) &R_list_filters,     0},
    {"R_list_muxers",      (DL_FUNC) &R_list_muxers,      0},
    {"R_log_level",        (DL_FUNC) &R_log_level,        1},
    {"R_video_info",       (DL_FUNC) &R_video_info,       1},
    {NULL, NULL, 0}
  };

  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
}
