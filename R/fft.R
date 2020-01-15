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
#' dim(read_audio_fft(wonderland))
#' dim(read_audio_fft(wonderland, hamming(4096)))
#' dim(read_audio_fft(wonderland, sample_rate = 8000))
read_audio_fft <- function(audio, window = hanning(1024), overlap = 0.75, sample_rate = NULL){
  audio <- normalizePath(audio, mustWork = TRUE)
  overlap <- as.numeric(overlap)
  sample_rate <- as.integer(sample_rate)
  if(!is.numeric(window) || length(window) < 256)
    stop("Window must have at least length 256")
  window <- as.numeric(window)
  if(!length(overlap) || overlap < 0 || overlap >= 1)
    stop("Overlap must be value between 0 and 1")
  info <- av_video_info(audio)
  out <- .Call(R_audio_fft, audio, window, overlap, sample_rate)
  attr(out, 'time') <- seq(0, info$duration, length.out = ncol(out))
  attr(out, 'frequency') <-  seq(0, info$audio$sample_rate * 0.5, length.out = nrow(out))
  attr(out, 'input') <- as.list(info$audio)
  structure(out, class = 'av_fft')
}

#' @export
plot.av_fft <- function(x, y, dark = TRUE, legend = TRUE, ...){
  oldpar <- graphics::par(no.readonly = TRUE)
  on.exit(graphics::par(oldpar))
  col <- if(isTRUE(dark)){
    graphics::par(bg='black', col.axis='white', fg='white', family='mono',
                  font=2, col.lab='white', col.main='white')
    #rev(viridisLite::inferno(12))
    c("#FCFFA4FF", "#F5DB4BFF", "#FCAD12FF", "#F78311FF", "#E65D2FFF", "#CB4149FF",
             "#A92E5EFF", "#85216BFF", "#60136EFF", "#3A0963FF", "#140B35FF", "#000004FF")
  } else {
    grDevices::hcl.colors(12, "YlOrRd", rev = TRUE)
  }
  graphics::par(mar=c(5, 5, 3, 3), mex=0.6)
  graphics::image(attr(x, 'time'), attr(x, 'frequency'), t(x),
                  xlab = 'TIME', ylab = 'FREQUENCY (HZ)', col = col, useRaster = TRUE)
  if(isTRUE(legend)){
    input <- attr(x, 'input')
    label <- sprintf("%d channel, %dHz", input$channels, input$sample_rate)
    graphics::legend("topright", legend = label, pch='', xjust = 1, yjust = 1, bty='o', cex = 0.7)
  }
}
