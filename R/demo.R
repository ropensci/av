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
av_demo <- function(output = "demo.mp4", width = 960, height = 720, framerate = 5, verbose = TRUE, ...){
  wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av', mustWork = TRUE)
  info <- av_media_info(wonderland)
  len <- framerate * round(info$duration)
  res <- round(72 * min(width, height) / 480)

  # RColorBrewer::brewer.pal(12,"Set3")
  col <- c("#8DD3C7", "#FFFFB3", "#BEBADA", "#FB8072", "#80B1D3", "#FDB462",
    "#B3DE69", "#FCCDE5", "#D9D9D9", "#BC80BD", "#CCEBC5", "#FFED6F")
  video <- av_capture_graphics(output = output,  audio = wonderland, {
    for(i in seq_len(len)){
      if(as.logical(verbose))
        cat(sprintf("\rGenerating plot %d/%d...", i, len), file = stderr())
      graphics::hist(stats::rnorm(100), col = col, main = sprintf("frame %d/%d", i, len))
    }
    if(as.logical(verbose))
      cat("done!\n", file = stderr())
  }, width = width, height = height, res = res, framerate = framerate, verbose = verbose, ...)
  if(interactive())
    utils::browseURL(video)
  return(video)
}
