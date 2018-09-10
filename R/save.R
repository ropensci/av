#' Save Video from Graphics Device
#'
#' Runs the expression and captures all plots into a video.
#'
#' @export
#' @rdname av_save_video
#' @inheritParams av_encode_video
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
#' # Play 1 plot per sec
#' video_file <- file.path(tempdir(), 'output.mp4')
#' av_save_video(makeplot(), video_file, 1280, 720, res = 144, verbose = FALSE)
#' utils::browseURL(video_file)}
av_save_video <- function(expr, output = 'output.mp4', width = 720, height = 480, framerate = 1,
                       filter = "null", verbose = TRUE, ...){
  imgdir <- tempfile('tmppng')
  dir.create(imgdir)
  on.exit(unlink(imgdir, recursive = TRUE))
  filename <- file.path(imgdir, "tmpimg_%05d.png")
  grDevices::png(filename, width = width, height = height, ...)
  graphics::par(ask = FALSE)
  tryCatch(eval(expr), finally = dev.off())
  images <- list.files(imgdir, pattern = 'tmpimg_\\d{5}.png', full.names = TRUE)
  av_encode_video(images, output = output, framerate = framerate, filter = filter, verbose = verbose)
}
