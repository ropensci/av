#' Demo Video
#'
#' Generates random video for testing purposes.
#'
#' @export
#' @inheritParams encoding
#' @inheritParams capturing
#' @param ... other parameters passed to [av_capture_graphics].
#' @name demo
#' @family av
av_demo <- function(width = 960, height = 720, framerate = 5, verbose = TRUE, ...){
  wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av', mustWork = TRUE)
  info <- av_video_info(wonderland)
  len <- framerate * round(info$duration)
  res <- round(72 * min(width, height) / 480)
  video <- av_capture_graphics({
    for(i in seq_len(len)){
      if(as.logical(verbose))
        cat(sprintf("\rGenerating plot %d/%d...", i, len), file = stderr())
      graphics::hist(stats::rnorm(100, mean = i), main = sprintf("frame %d/%d", i, len))
    }
    if(as.logical(verbose))
      cat("done!\n", file = stderr())
  }, width = width, height = height, res = res, framerate = framerate, audio = wonderland, verbose = verbose, ...)
  utils::browseURL(video)
}
