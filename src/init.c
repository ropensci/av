#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <R_ext/Rdynload.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavformat/version.h>

/* In theory this should be thread safe and R is not */
void my_log_callback(void *ptr, int level, const char *fmt, va_list vargs){
  if(level <= av_log_get_level())
    REvprintf(fmt, vargs);
}

SEXP R_log_level(SEXP new_level){
  if(Rf_length(new_level))
    av_log_set_level(Rf_asInteger(new_level));
  return Rf_ScalarInteger(av_log_get_level());
}

void R_init_av(DllInfo *info) {
#if LIBAVFORMAT_VERSION_MAJOR < 58 // FFmpeg 4.0
  av_register_all();
  avcodec_register_all();
#endif
  avformat_network_init();
  av_log_set_callback(my_log_callback);
}
