#' AV Formats
#'
#' List supported filters, codecs and container formats.
#'
#' Encoders and decoders convert between raw video/audio frames and compressed stream
#' data for storage or transfer. However such a compressed data stream by itself does
#' not constitute a valid video format yet. Muxers are needed to interleave one or more
#' audio/video/subtitle streams, along with timestamps, metadata, etc, into a proper
#' file format, such as mp4 or mkv.
#'
#' Conversely, demuxers are needed to read a file format into the seperate data streams
#' for subsequent decoding into raw audio/video frames. Most operating systems natively
#' support demuxing and decoding common formats and codecs, needed to play those videos.
#' However for encoding and muxing such videos, ffmpeg must have been configured with
#' specific external libraries for a given codec or format.
#'
#' @export
#' @rdname formats
#' @name formats
#' @family av
av_encoders <- function(){
  list_codecs(TRUE)
}

#' @export
#' @rdname formats
av_decoders <- function(){
  list_codecs(FALSE)
}

#' @useDynLib av R_list_codecs
list_codecs <- function(is_encoder){
  res <- as.list(.Call(R_list_codecs))
  names(res) <- c("type", "encoder", "name", "description","format")
  df <- data.frame(res, stringsAsFactors = FALSE)
  df <- df[df$encoder == is_encoder, c("type", "name", "description", "format")]
  df <- df[order(df$name),]
  row.names(df) <- NULL
  df
}

#' @rdname formats
#' @export
#' @useDynLib av R_list_filters
av_filters <- function(){
  res <-as.list(.Call(R_list_filters))
  names(res) <- c("name", "description")
  df <- data.frame(res, stringsAsFactors = FALSE)
  df <- df[order(df$name),]
  row.names(df) <- NULL
  df
}

#' @rdname formats
#' @export
#' @useDynLib av R_list_muxers
av_muxers <- function(){
  res <-as.list(.Call(R_list_muxers))
  names(res) <- c("name", "mime", "extensions", "audio", "video", "description")
  df <- data.frame(res, stringsAsFactors = FALSE)
  df <- df[order(df$name),]
  row.names(df) <- NULL
  df
}

#' @rdname formats
#' @export
#' @useDynLib av R_list_demuxers
av_demuxers <- function(){
  res <-as.list(.Call(R_list_demuxers))
  names(res) <- c("name", "mime", "extensions", "description")
  df <- data.frame(res, stringsAsFactors = FALSE)
  df <- df[order(df$name),]
  row.names(df) <- NULL
  df
}

