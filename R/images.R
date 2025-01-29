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
#' @param trim string value for [ffmpeg trim filter](https://ffmpeg.org/ffmpeg-filters.html#trim)
#' for example `"10:15"` for seconds or `"start_frame=100:end_frame=110"` for frames.
#' @param format image format such as `png` or `jpeg`, must be available from `av_encoders()`
#' @examples \dontrun{
#' curl::curl_download('https://jeroen.github.io/images/blackbear.mp4', 'blackbear.mp4')
#' av_video_images('blackbear.mp4', fps = 1, trim = "10:20")
#' }
av_video_images <- function(video, destdir = tempfile(), format = 'jpg', fps = NULL, trim = NULL){
  stopifnot(length(video) == 1)
  filter_fps <- if(length(fps)) paste0('fps=fps=', fps)
  filter_trim <- if(length(trim)) paste0('trim=', trim)
  vfilter <- paste(c(filter_trim, filter_fps), collapse = ', ')
  if(vfilter == "")
    vfilter <- 'null'
  framerate <- av_media_info(video)$video$framerate
  dir.create(destdir)
  codec <- switch(format, jpeg = 'mjpeg', jpg = 'mjpeg', format)
  output <- file.path(destdir, paste0('image_%6d.', format))
  av_encode_video(input = video, output = output, framerate = framerate,
                  codec = codec, vfilter = vfilter)
  list.files(destdir, pattern = paste0('image_\\d{6}.', format), full.names = TRUE)
}
