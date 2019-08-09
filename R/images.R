#' Split Video into PNG
#'
#' Convert a video file in a set of PNG images.
#'
#' @export
#' @param video an input video
#' @param destdir directory where to save the png files
#' @param fps sample rate of images. Use `NULL` to get all images.
av_video_images <- function(video, destdir = tempfile(), fps = NULL){
  stopifnot(length(video) == 1)
  vfilter <- ifelse(length(fps), paste0('fps=fps=', fps), 'null')
  framerate <- av_video_info(video)$video$framerate
  dir.create(destdir)
  output <- file.path(destdir, 'image_%6d.jpg')
  av_encode_video(input = video, output = output, framerate = framerate, vfilter = vfilter)
  list.files(destdir, pattern = 'image_\\d{6}.jpg', full.names = TRUE)
}
