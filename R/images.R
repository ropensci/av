#' Convert video to images
#'
#' Splits a video file in a set of image files. Default image format is
#' jpeg which has good speed and compression. Use `format = "png"` for
#' losless images.
#'
#' For large input videos you can set fps to sample only a limited number
#' of images per second. This also works with fractions, for example `fps = 0.2`
#' will output one image for every 5 sec of video.
#'
#' @export
#' @param video an input video
#' @param destdir directory where to save the png files
#' @param fps sample rate of images. Use `NULL` to get all images.
#' @param format image format such as `png` or `jpeg`, must be available from `av_encoders()`
av_video_images <- function(video, destdir = tempfile(), format = 'jpg', fps = NULL){
  stopifnot(length(video) == 1)
  vfilter <- ifelse(length(fps), paste0('fps=fps=', fps), 'null')
  framerate <- av_video_info(video)$video$framerate
  dir.create(destdir)
  codec <- switch(format, jpeg = 'mjpeg', jpg = 'mjpeg', format)
  output <- file.path(destdir, paste0('image_%6d.', format))
  av_encode_video(input = video, output = output, framerate = framerate,
                  codec = codec, vfilter = vfilter)
  list.files(destdir, pattern = paste0('image_\\d{6}.', format), full.names = TRUE)
}
