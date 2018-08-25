#' Convert to Video
#'
#' Convert Images into Video
#'
#' @export
#' @useDynLib av R_convert
av_convert <- function(input, output){
  input <- normalizePath(input, mustWork = TRUE)
  output <- normalizePath(output, mustWork = FALSE)
  .Call(R_convert, input, output)
}
