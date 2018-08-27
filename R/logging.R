#' Logging
#' 
#' Get or set the [log level](https://www.ffmpeg.org/doxygen/4.0/group__lavu__log__constants.html).
#' 
#' @useDynLib av R_log_level
#' @export
#' @param set new [log level](https://www.ffmpeg.org/doxygen/4.0/group__lavu__log__constants.html) value
av_log_level <- function(set = NULL){
  .Call(R_log_level, set)
}
