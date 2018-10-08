#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <libavdevice/avdevice.h>

static SEXP safe_string(const char *x){
  if(x == NULL)
    return NA_STRING;
  return Rf_mkChar(x);
}

SEXP R_list_input_devices(){
  AVInputFormat *input = NULL;
  int count = 0;
  while((input = av_input_video_device_next(input)))
    count++;
  while((input = av_input_audio_device_next(input)))
    count++;
  SEXP name = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP desc = PROTECT(Rf_allocVector(STRSXP, count));
  SEXP video = PROTECT(Rf_allocVector(LGLSXP, count));

  // Re-iterate from scratch
  int i = 0;
  while((input = av_input_video_device_next(input))) {
    SET_STRING_ELT(name, i, safe_string(input->name));
    SET_STRING_ELT(desc, i, safe_string(input->long_name));
    LOGICAL(video)[i] = TRUE;
    i++;
  }
  while((input = av_input_audio_device_next(input))) {
    SET_STRING_ELT(name, i, safe_string(input->name));
    SET_STRING_ELT(desc, i, safe_string(input->long_name));
    LOGICAL(video)[i] = FALSE;
    i++;
  }
  SEXP out = Rf_list3(name, video, desc);
  UNPROTECT(3);
  return out;
}
