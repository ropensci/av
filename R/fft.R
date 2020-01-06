#' Calculate FFT
#'
#' Convert audio into frequency domain.
#'
#' @export
#' @useDynLib av R_audio_fft
#' @param audio path to the audio file
#' @param win_size sample size of the moving fft window
#' @param overlap number between 0 and 1 of overlap proportion for moving fft window
#' @examples wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av')
#' av_audio_fft(wonderland, 4096)
av_audio_fft <- function(audio, win_size = 2048, overlap = 0.75){
  audio <- normalizePath(audio, mustWork = TRUE)
  win_size <- as.integer(win_size)
  overlap <- as.numeric(overlap)
  if(overlap < 0 || overlap >= 1)
    stop("Overlap must be between 0 and 1")
  .Call(R_audio_fft, audio, win_size, overlap)
}