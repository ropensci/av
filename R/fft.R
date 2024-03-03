#' Read audio binary and frequency data
#'
#' Reads raw audio data from any common audio or video format. Use [read_audio_bin] to
#' get raw PCM audio samples, or [read_audio_fft] to stream-convert directly into
#' frequency domain (spectrum) data using FFmpeg built-in FFT.
#'
#' Currently [read_audio_fft] automatically converts input audio to mono channel such
#' that we get a single matrix. Use the `plot()` method on data returned by [read_audio_fft]
#' to show the spectrogram. The [av_spectrogram_video] generates a video that plays
#' the audio while showing an animated spectrogram with moving status bar, which is
#' very cool.
#'
#' @export
#' @rdname read_audio
#' @family av
#' @useDynLib av R_audio_fft
#' @param audio path to the input sound or video file containing the audio stream
#' @param window vector with weights defining the moving [fft window function][hanning].
#' The length of this vector is the size of the window and hence determines the output
#' frequency range.
#' @param overlap value between 0 and 1 of overlap proportion between moving fft windows
#' @param sample_rate downsample audio to reduce FFT output size. Default keeps sample
#' rate from the input file.
#' @param start_time,end_time position (in seconds) to cut input stream to be processed.
#' @examples # Use a 5 sec fragment
#' wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av')
#'
#' # Read initial 5 sec as as frequency spectrum
#' fft_data <- read_audio_fft(wonderland, end_time = 5.0)
#' dim(fft_data)
#'
#' # Plot the spectrogram
#' plot(fft_data)
#'
#' # Show other parameters
#' dim(read_audio_fft(wonderland, end_time = 5.0, hamming(2048)))
#' dim(read_audio_fft(wonderland, end_time = 5.0, hamming(4096)))
read_audio_fft <- function(audio, window = hanning(1024), overlap = 0.75,
                           sample_rate = NULL, start_time = NULL, end_time = NULL){
  audio <- normalizePath(audio, mustWork = TRUE)
  overlap <- as.numeric(overlap)
  sample_rate <- as.integer(sample_rate)
  if(!is.numeric(window) || length(window) < 256)
    stop("Window must have at least length 256")
  window <- as.numeric(window)
  if(!length(overlap) || overlap < 0 || overlap >= 1)
    stop("Overlap must be value between 0 and 1")
  info <- av_media_info(audio)
  start_time <- as.numeric(start_time)
  end_time <- as.numeric(end_time)
  av_log_level(16)
  out <- .Call(R_audio_fft, audio, window, overlap, sample_rate, start_time, end_time)

  # Get the real start/end times
  if(!length(start_time) || start_time < 0)
    start_time = 0
  end_time <- attr(out, 'endtime')
  attr(out, 'endtime') = NULL
  attr(out, 'duration') = end_time - start_time;
  attr(out, 'time') <- seq(start_time, end_time, length.out = ncol(out))
  attr(out, 'frequency') <-  seq(0, info$audio$sample_rate * 0.5, length.out = nrow(out))
  attr(out, 'input') <- as.list(info$audio)
  structure(out, class = 'av_fft')
}

#' @export
#' @rdname read_audio
#' @useDynLib av R_audio_bin
#' @param channels number of output channels, set to 1 to convert to mono sound
read_audio_bin <- function(audio, channels = NULL, sample_rate = NULL, start_time = NULL, end_time = NULL){
  audio <- normalizePath(audio, mustWork = TRUE)
  channels <- as.integer(channels)
  sample_rate <- as.integer(sample_rate)
  start_time <- as.numeric(start_time)
  end_time <- as.numeric(end_time)
  av_log_level(16)
  .Call(R_audio_bin, audio, channels, sample_rate, start_time, end_time)
}

#' @export
#' @rdname read_audio
#' @param pcm_data integer vector as returned by [read_audio_bin]
#' @param pcm_channels number of channels in the data. Use the same value as you
#' entered in [read_audio_bin].
#' @param pcm_format this is always `s32le` (signed 32-bit integer) for now
#' @param output passed to [av_audio_convert]
#' @param ... other paramters for [av_audio_convert]
write_audio_bin <- function(pcm_data, pcm_channels = 1L, pcm_format = 's32le',
                            output = "output.mp3", ...){
  if(!is.integer(pcm_data))
    stop("Argument 'pcm_data' is supposed to be a integer vector obtained from read_audio_bin()")
  tmp <- structure(tempfile(), class = 'pcm', fmt = pcm_format, channels = pcm_channels)
  on.exit(unlink(tmp))
  writeBin(c(pcm_data), tmp)
  av_audio_convert(tmp, output = output, ...)
}

read_audio_bin_old <- function(audio, channels = NULL, sample_rate = NULL, start_time = NULL, total_time = NULL){
  tmp <- tempfile(fileext = '.bin')
  on.exit(unlink(tmp))
  audio <- normalizePath(audio, mustWork = TRUE)
  channels <- as.integer(channels)
  av_audio_convert(audio = audio, output = tmp, format = 's32le', channels = channels,
                   sample_rate = sample_rate, start_time = start_time, total_time = total_time, verbose = FALSE)
  out <- readBin(tmp, integer(), file.info(tmp)$size)
  out[is.na(out)] <- -2147483647L #-(.Machine$integer.max)
  if(!length(channels))
    channels <- av_media_info(audio)$audio$channels
  if(!length(sample_rate))
    sample_rate <- av_media_info(audio)$audio$sample_rate
  structure(out, channels = channels, sample_rate = sample_rate)
}

## Do not remove importfrom! https://github.com/ropensci/av/issues/29
#' @importFrom graphics par image legend abline
#' @export
plot.av_fft <- function(x, y, dark = TRUE, legend = TRUE, keep.par = FALSE, useRaster = TRUE, vline = NULL, ...){
  if(!isTRUE(keep.par)){
    # This will also reset the coordinate scale of the plot
    oldpar <- par(no.readonly = TRUE)
    on.exit(par(oldpar))
  }
  col <- if(isTRUE(dark)){
    par(bg='black', col.axis='white', fg='white', family='mono',
                  font=2, col.lab='white', col.main='white')
    #rev(viridisLite::inferno(12))
    c("#FCFFA4FF", "#F5DB4BFF", "#FCAD12FF", "#F78311FF", "#E65D2FFF", "#CB4149FF",
             "#A92E5EFF", "#85216BFF", "#60136EFF", "#3A0963FF", "#140B35FF", "#000004FF")
  } else {
    # hcl.colors(12, "YlOrRd", rev = TRUE)
    c("#FFFFC8", "#FFF4B7", "#FBE49A", "#F8D074", "#F7BA3C", "#F5A100",
      "#F28400", "#ED6200", "#E13C00", "#C32200", "#A20706", "#7D0025")
  }
  par(mar=c(5, 5, 3, 3), mex=0.6)
  image(attr(x, 'time'), attr(x, 'frequency'), t(x),
                  xlab = 'TIME', ylab = 'FREQUENCY (HZ)', col = col, useRaster = useRaster)
  if(isTRUE(legend)){
    input <- attr(x, 'input')
    label <- sprintf("%d channel, %dHz", input$channels, input$sample_rate)
    legend("topright", legend = label, pch='', xjust = 1, yjust = 1, bty='o', cex = 0.7)
  }
  if(length(vline)){
    abline(v = vline, lwd = 2)
  }
}
