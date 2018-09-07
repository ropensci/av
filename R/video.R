#' Create Video
#'
#' Encodes a series of images into a video.
#'
#' @export
#' @useDynLib av R_encode_video
#' @rdname av_convert
#' @param input a vector with images such as png files.
#' @param output name of the output file. File extension must correspond to a known
#' container format such as `mp4`, `mkv`, `mov`, or `flv`.
#' @param filter a string defining an ffmpeg filter graph
#' @param framerate video framerate in frames per seconds.
#' @param codec name of the video codec as listed in [av_encoders][av_encoders]. Most
#' formats default to h264 which has excellent compression and works out of the box on
#' all [modern browsers](https://caniuse.com/#search=h264) and operating systems.
#' @param verbose emit some output and a progress meter counting processed images. Must
#' be `TRUE` or `FALSE` or an integer with a valid [log_level](av_log_level).
av_encode_video <- function(input, output = "video.mp4", framerate = 1, filter = "null", codec = NULL, verbose = TRUE){
  stopifnot(length(input) > 0)
  input <- normalizePath(input, mustWork = TRUE)
  stopifnot(length(output) == 1)
  output <- normalizePath(output, mustWork = FALSE)
  stopifnot(file.exists(dirname(output)))
  stopifnot(length(framerate) == 1)
  framerate <- as.numeric(framerate)
  filter <- as.character(filter)
  codec <- as.character(codec)
  if(is.logical(verbose))
    verbose <- ifelse(isTRUE(verbose), 32, 16)
  old_log_level <- av_log_level()
  on.exit(av_log_level(old_log_level))
  av_log_level(verbose)
  .Call(R_encode_video, input, output, framerate, filter, codec)
}
