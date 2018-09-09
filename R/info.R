#' Video Info
#'
#' Get video info
#'
#' @export
#' @param file path to an existing file
#' @useDynLib av R_video_info
#' @family av
av_video_info <- function(file){
  out <- .Call(R_video_info, file)
  out$video <- data.frame(out$video, stringsAsFactors = FALSE)
  out
}
