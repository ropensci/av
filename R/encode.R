#' Encode or Convert Audio / Video
#'
#' Encodes a set of images into a video, using custom container format, codec, fps,
#' [video filters](https://ffmpeg.org/ffmpeg-filters.html#Video-Filters), and audio
#' track. If input contains video files, this effectively combines and converts them
#' to the specified output format.
#'
#' The target container format and audio/video codes are automatically determined from
#' the file extension of the output file, for example `mp4`, `mkv`, `mov`, or `flv`.
#' For video output, most systems also support `gif` output, but the compression~quality
#' for gif is really bad. The [gifski](https://cran.r-project.org/package=gifski) package
#' is better suited for generating animated gif files. Still using a proper video format
#' is results in much better quality.
#'
#' It is recommended to use let ffmpeg choose the suitable codec for a given container
#' format. Most video formats default to the `libx264` video codec which has excellent
#' compression and works on all modern browsers, operating systems, and digital TVs.
#'
#' To convert from/to raw PCM audio, use file extensions `".ub"` or `".sb"` for 8bit
#' unsigned or signed respectively, or `".uw"` or `".sw"` for 16-bit, see `extensions`
#' in [av_muxers()]. Alternatively can also convert to other raw audio PCM by setting
#' for example `format = "u16le"` (i.e. unsigned 16-bit little-endian) or another option
#' from the `name` column in [av_muxers()].
#'
#' It is safe to interrupt the encoding process by pressing CTRL+C, or via [setTimeLimit].
#' When the encoding is interrupted, the output stream is properly finalized and all open
#' files and resources are properly closed.
#'
#' @export
#' @aliases av
#' @family av
#' @name encoding
#' @useDynLib av R_encode_video
#' @rdname encoding
#' @param input a vector with image or video files. A video input file is treated
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
#' be `TRUE` or `FALSE` or an integer with a valid [av_log_level].
av_encode_video <- function(input, output = "output.mp4", framerate = 24, vfilter = "null",
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
  audio <- as.character(audio)
  if(is.logical(verbose))
    verbose <- ifelse(isTRUE(verbose), 32, 16)
  old_log_level <- av_log_level()
  on.exit(av_log_level(old_log_level), add = TRUE)
  av_log_level(verbose)
  .Call(R_encode_video, input, output, framerate, vfilter, codec, audio)
}

#' @rdname encoding
#' @export
#' @param video input video file with optionally also an audio track
av_video_convert <- function(video, output = "output.mp4", verbose = TRUE){
  info <- av_media_info(video)
  if(nrow(info$video) == 0)
    stop("No suitable input video stream found")
  framerate <- info$video$framerate[1]
  audio <- if(length(info$audio) && nrow(info$audio)) video
  av_encode_video(input = video, audio = audio, output = output,
                  framerate = framerate, verbose = verbose)
}

#' @rdname encoding
#' @export
#' @useDynLib av R_convert_audio
#' @param channels number of output channels. Default `NULL` will match input.
#' @param sample_rate output sampling rate. Default `NULL` will match input.
#' @param bit_rate output bitrate (quality). A common value is 192000. Default
#' `NULL` will match input.
#' @param format a valid output format name from the list of `av_muxers()`. Default
#' `NULL` infers format from the file extension.
#' @param start_time number greater than 0, seeks in the input file to position.
#' @param total_time approximate number of seconds at which to limit the duration
#' of the output file.
av_audio_convert <- function(audio, output = 'output.mp3', format = NULL,
                             channels = NULL, sample_rate = NULL, bit_rate = NULL,
                             start_time = NULL, total_time = NULL, verbose = interactive()){
  stopifnot(length(audio) > 0)
  input <- normalizePath(audio, mustWork = TRUE)
  attributes(input) <- attributes(audio)
  stopifnot(length(output) == 1)
  output <- normalizePath(output, mustWork = FALSE)
  stopifnot(file.exists(dirname(output)))
  format <- as.character(format)
  if(length(channels))
    stopifnot(is.numeric(channels))
  if(length(sample_rate))
    stopifnot(is.numeric(sample_rate))
  if(length(bit_rate))
    stopifnot(is.numeric(bit_rate))
  if(is.logical(verbose))
    verbose <- ifelse(isTRUE(verbose), 32, 16)
  if(length(start_time))
    stopifnot(is.numeric(start_time))
  if(length(total_time))
    stopifnot(is.numeric(total_time))
  old_log_level <- av_log_level()
  on.exit(av_log_level(old_log_level), add = TRUE)
  av_log_level(verbose)
  .Call(R_convert_audio, input, output, format, channels, sample_rate, bit_rate, start_time, total_time)
}
