#' Logging
#'
#' Get or set the [log level](https://www.ffmpeg.org/doxygen/4.0/group__lavu__log__constants.html).
#'
#' @useDynLib av R_log_level
#' @family av
#' @name logging
#' @export
#' @param set new [log level](https://www.ffmpeg.org/doxygen/4.0/group__lavu__log__constants.html) value
av_log_level <- function(set = NULL){
  if(length(set)){
    levels <- seq(-8, 56, 8)
    if(!(set %in% levels))
      stop("Log level must be one of: ", paste(levels, collapse = ", "))
  }
  .Call(R_log_level, set)
}

#' @useDynLib av R_get_open_handles
get_open_handles <- function(){
  .Call(R_get_open_handles)
}
