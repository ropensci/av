#' Calculate FFT
#'
#' Convert audio into frequency domain.
#'
#' @useDynLib av R_audio_fft
#' @inheritParams encoding
#' @export
av_audio_fft <- function(audio){
  audio <- normalizePath(audio, mustWork = TRUE)
  .Call(R_audio_fft, audio)
}
