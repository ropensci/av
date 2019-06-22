#' Record Video from Graphics Device
#'
#' Runs the expression and captures all plots into a video.
#'
#' @export
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
#' av::av_video_info(video_file)
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
