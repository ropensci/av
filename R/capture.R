#' Record Video from Graphics Device
#'
#' Runs the expression and captures all plots into a video. The [av_spectrogram_video]
#' function is a wrapper that plots data from [read_audio_fft] with a moving bar and
#' background audio.
#'
#' @export
#' @rdname capturing
#' @name capturing
#' @family av
#' @inheritParams encoding
#' @param expr an R expression that generates the graphics to capture
#' @param width width in pixels of the graphics device
#' @param height height in pixels of the graphics device
#' @param ... extra graphics parameters passed to [png()]
#' @examples
#' \donttest{
#' library(gapminder)
#' library(ggplot2)
#' makeplot <- function(){
#'   datalist <- split(gapminder, gapminder$year)
#'   lapply(datalist, function(data){
#'     p <- ggplot(data, aes(gdpPercap, lifeExp, size = pop, color = continent)) +
#'       scale_size("population", limits = range(gapminder$pop)) + geom_point() + ylim(20, 90) +
#'       scale_x_log10(limits = range(gapminder$gdpPercap)) + ggtitle(data$year) + theme_classic()
#'     print(p)
#'   })
#' }
#'
#' # Play 1 plot per sec, and use an interpolation filter to convert into 10 fps
#' video_file <- file.path(tempdir(), 'output.mp4')
#' av_capture_graphics(makeplot(), video_file, 1280, 720, res = 144, vfilter = 'framerate=fps=10')
#' av::av_media_info(video_file)
#' # utils::browseURL(video_file)}
av_capture_graphics <- function(expr, output = 'output.mp4', width = 720, height = 480, framerate = 1,
                       vfilter = "null", audio = NULL, verbose = TRUE, ...){
  imgdir <- tempfile('tmppng')
  dir.create(imgdir)
  on.exit(unlink(imgdir, recursive = TRUE))
  filename <- file.path(imgdir, "tmpimg_%05d.png")
  grDevices::png(filename, width = width, height = height, ...)
  graphics::par(ask = FALSE)
  tryCatch(eval(expr), finally = grDevices::dev.off())
  images <- list.files(imgdir, pattern = 'tmpimg_\\d{5}.png', full.names = TRUE)
  av_encode_video(images, output = output, framerate = framerate, vfilter = vfilter, audio = audio, verbose = verbose)
}

#' @export
#' @param audio path to media file with audio stream
#' @rdname capturing
av_spectrogram_video <- function(audio, output = "output.mp4", framerate = 25, verbose = interactive(), ...){
  fftdata <- read_audio_fft(audio)
  duration <- attr(fftdata, 'duration')
  movie <- av_capture_graphics({
    for(i in seq(0, duration, by = 1/framerate)){
      cat(sprintf("\rPlotting frame at %.2f sec...", i), file = stderr())
      graphics::plot(fftdata, vline = i)
    }
  }, framerate = framerate, audio = audio, verbose = verbose, output = output, ...)
}
