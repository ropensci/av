#' Read audio binary and frequency data
#'
#' Fast implementation to read audio files in any format. Use [read_audio_bin] to read
#' raw audio samples, and [read_audio_fft] to stream-convert directly to frequency domain
#' (spectrum data) using FFmpeg built-in FFT.
#'
#' Use the `plot()` method on data returned by [read_audio_fft] to show the spectrogram.
#' The [av_spectrogram_video] generates a video based on the spectrogram with a moving bar
#' and background audio.
#'
#' @export
#' @rdname read_audio
#' @useDynLib av R_audio_fft
#' @param audio path to the audio file
#' @param window vector with weights defining the moving [fft window function][hanning].
#' The length of this vector is the size of the window and hence determines the output
#' frequency range.
#' @param overlap value between 0 and 1 of overlap proportion between moving fft windows
#' @param sample_rate downsample audio to reduce FFT output size. Default keeps sample
#' rate from the input file.
#' @examples # Use a 5 sec fragment
#' wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av')
#' av_audio_convert(wonderland, 'example.mp3', total_time = 5)
#'
#' # Read as frequency spectrum
#' fft_data <- read_audio_fft('example.mp3')
#' dim(fft_data)
#'
#' # Plot the spectrogram
#' plot(fft_data)
#'
#' # Show other parameters
#' dim(read_audio_fft('example.mp3', hamming(2048)))
#' dim(read_audio_fft('example.mp3', hamming(4096)))
#'
#' # Cleanup
#' unlink('example.mp3')
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
#' @rdname read_audio
#' @param channels number of output channels, set to 1 to conver to mono sound
read_audio_bin <- function(audio, channels = NULL, sample_rate = NULL){
  tmp <- tempfile(fileext = '.bin')
  on.exit(unlink(tmp))
  av_audio_convert(audio = audio, output = tmp, format = 's32le',
                   channels = channels, sample_rate = sample_rate, verbose = FALSE)
  out <- readBin(tmp, integer(), file.info(tmp)$size)
  if(!length(channels))
    channels <- av_video_info(audio)$audio$channels
  structure(out, channels = channels)
}

#' @export
plot.av_fft <- function(x, y, dark = TRUE, legend = TRUE, keep.par = FALSE, useRaster = TRUE, vline = NULL, ...){
  if(!isTRUE(keep.par)){
    # This will also reset the coordinate scale of the plot
    oldpar <- graphics::par(no.readonly = TRUE)
    on.exit(graphics::par(oldpar))
  }
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
                  xlab = 'TIME', ylab = 'FREQUENCY (HZ)', col = col, useRaster = useRaster)
  if(isTRUE(legend)){
    input <- attr(x, 'input')
    label <- sprintf("%d channel, %dHz", input$channels, input$sample_rate)
    graphics::legend("topright", legend = label, pch='', xjust = 1, yjust = 1, bty='o', cex = 0.7)
  }
  if(length(vline)){
    graphics::abline(v = vline, lwd = 2)
  }
}
