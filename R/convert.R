#' Create Video
#'
#' Converts a set of images into a video.
#'
#' @export
#' @useDynLib av R_create_video
#' @rdname av_convert
#' @param input a vector with images such as png files
#' @param output name of the output file. File extension must correspond to a known
#' container format such as `mp4` `mkv` `mov` or `flv`
#' @param width output video width in pixels. Defaults to width of first input image
#' @param height output video height in pixels. Defaults to height of first input image
#' @param fps output framerate in frames per seconds.
create_video <- function(input, output = "video.mp4", width = NULL, height = NULL, fps = 1){
  stopifnot(length(input) > 0)
  input <- normalizePath(input, mustWork = TRUE)
  stopifnot(length(output) > 0)
  output <- normalizePath(output, mustWork = FALSE)
  stopifnot(file.exists(dirname(output)))
  width <- as.integer(width)
  height <- as.integer(height)
  stopifnot(length(fps) > 0)
  fps <- as.integer(fps)
  .Call(R_create_video, input, output, width, height, fps)
}
