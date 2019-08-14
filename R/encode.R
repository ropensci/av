#' Encode or Convert Video
#'
#' Encodes a set of images into a video, using custom container format, codec, fps,
#' [video filters](https://ffmpeg.org/ffmpeg-filters.html#Video-Filters), and audio
#' track. If input contains video files, this effectively combines and converts them
#' to the specified output format.
#'
#' The target container format is automatically determined from the file extension of
#' the output file, for example `mp4`, `mkv`, `mov`, or `flv`. Most systems also
#' support `gif` output, but the compression~quality for gif is quite bad.
#' The [gifski](https://cran.r-project.org/package=gifski) package is better suited
#' for generating animated gif files.
#'
#' It is recommended to use let ffmpeg choose the suitable codec for a given container
#' format. Most video formats default to the `libx264` video codec which has excellent
#' compression and works on all modern [browsers](https://caniuse.com/#search=h264),
#' operating systems, and digital TVs.
#'
#' @export
#' @aliases av
#' @family av
#' @name encoding
#' @useDynLib av R_encode_video
#' @rdname av_encode_video
#' @param input a vector with images such as png files. A video input file is treated
#' as a series of images. All input files should have the same width and height.
#' @param output name of the output file. File extension must correspond to a known
#' container format such as `mp4`, `mkv`, `mov`, or `flv`.
#' @param vfilter a string defining an ffmpeg filter graph. This is the same parameter
#' as the `-vf` argument in the `ffmpeg` command line utility.
#' @param framerate video framerate in frames per seconds. This is the input fps, the
#' output fps may be different if you specify a filter that modifies speed or interpolates
#' frames.
#' @param codec name of the video codec as listed in [av_encoders][av_encoders]. The
#' default is `libx264` for most formats, which usually the best choice.
#' @param audio audio or video input file with sound for the output video
#' @param verbose emit some output and a progress meter counting processed images. Must
#' be `TRUE` or `FALSE` or an integer with a valid [log_level](av_log_level).
av_encode_video <- function(input, output = "video.mp4", framerate = 24, vfilter = "null",
                            codec = NULL, audio = NULL, verbose = TRUE){
  stopifnot(length(input) > 0)
  input <- normalizePath(input, mustWork = TRUE)
  stopifnot(length(output) == 1)
  output <- normalizePath(output, mustWork = FALSE)
  stopifnot(file.exists(dirname(output)))
  stopifnot(length(framerate) == 1)
  framerate <- as.numeric(framerate)
  vfilter <- as.character(vfilter)
  codec <- as.character(codec)
  if(length(audio))
    audio <- normalizePath(audio, mustWork = TRUE)
  if(is.logical(verbose))
    verbose <- ifelse(isTRUE(verbose), 32, 16)
  old_log_level <- av_log_level()
  on.exit(av_log_level(old_log_level), add = TRUE)
  av_log_level(verbose)
  .Call(R_encode_video, input, output, framerate, vfilter, codec, audio)
}

#' @rdname av_encode_video
#' @export
#' @useDynLib av R_encode_audio
av_encode_audio <- function(audio, output = 'output.mp3', verbose = TRUE){
  stopifnot(length(audio) > 0)
  audio <- normalizePath(audio, mustWork = TRUE)
  stopifnot(length(output) == 1)
  output <- normalizePath(output, mustWork = FALSE)
  stopifnot(file.exists(dirname(output)))
  if(is.logical(verbose))
    verbose <- ifelse(isTRUE(verbose), 32, 16)
  old_log_level <- av_log_level()
  on.exit(av_log_level(old_log_level), add = TRUE)
  av_log_level(verbose)
  .Call(R_encode_audio, audio, output)
}
