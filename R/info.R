#' Video Info
#'
#' Get video info such as width, height, format, duration and framerate.
#' This may also be used for audio input files.
#'
#' @export av_video_info av_media_info
#' @aliases av_video_info av_media_info
#' @name info
#' @param file path to an existing file
#' @useDynLib av R_video_info
#' @family av
av_media_info <- function(file){
  file <- normalizePath(file, mustWork = TRUE)
  out <- .Call(R_video_info, file)
  if(length(out$video))
    out$video <- data.frame(out$video, stringsAsFactors = FALSE)
  if(length(out$audio))
    out$audio <- data.frame(out$audio, stringsAsFactors = FALSE)
  out
}

av_video_info <- av_media_info
