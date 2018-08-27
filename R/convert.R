#' Convert to Video
#'
#' Convert Images into Video
#'
#' @export
#' @useDynLib av R_convert
#' @param input vector with image or video files
#' @param output filename of the output file. Container type is inferred from
#' the file extension which should typically be `.mkv` or `.mp4`.
av_convert <- function(input, output){
  input <- normalizePath(input, mustWork = TRUE)
  output <- normalizePath(output, mustWork = FALSE)
  .Call(R_convert, input, output)
}
