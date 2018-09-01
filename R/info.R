#' Video Info
#'
#' Get video info
#'
#' @export
#' @param file path to an existing file
#' @useDynLib av R_video_info
av_video_info <- function(file){
  out <- .Call(R_video_info, file)
  out$video_streams <- data.frame(out$video_streams, stringsAsFactors = FALSE)
  out
}
