#' Read audio frequency data
#'
#' Very fast implementation to read arbitrary audio file format and directly
#' stream-convert to frequency domain (spectrum data) using FFmpeg built-in FFT.
#'
#' @export
#' @useDynLib av R_audio_fft
#' @param audio path to the audio file
#' @param window vector with weights defining the moving [fft window function][hanning].
#' The length of this vector is the size of the window and hence determines the output
#' frequency range.
#' @param overlap value between 0 and 1 of overlap proportion between moving fft windows
#' @param sample_rate downsample audio to reduce FFT output size. Default keeps sample
#' rate from the input file.
#' @examples wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av')
#' dim(av_audio_fft(wonderland))
#' dim(av_audio_fft(wonderland, hamming(4096)))
#' dim(av_audio_fft(wonderland, sample_rate = 8000))
av_audio_fft <- function(audio, window = hanning(2048), overlap = 0.75, sample_rate = NULL){
  audio <- normalizePath(audio, mustWork = TRUE)
  overlap <- as.numeric(overlap)
  sample_rate <- as.integer(sample_rate)
  if(!is.numeric(window) || length(window) < 256)
    stop("Window must have at least length 256")
  window <- as.numeric(window)
  if(!length(overlap) || overlap < 0 || overlap >= 1)
    stop("Overlap must be value between 0 and 1")
  .Call(R_audio_fft, audio, window, overlap, sample_rate)
}
