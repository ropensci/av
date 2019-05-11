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
av_demo <- function(output = "demo.mp4", width = 960, height = 720, framerate = 24, verbose = TRUE, ...){
  big_horns_intro <- system.file('samples/Big_Horns_Intro.mp3', package='av', mustWork = TRUE)
  info <- av_video_info(big_horns_intro)
  len <- framerate * round(info$duration)
  res <- round(72 * min(width, height) / 480)

  x <- seq(-6, 6, length = 100)
  y <- x

  # Himmelblau's function: https://en.wikipedia.org/wiki/Himmelblau%27s_function
  f <- function(x, y) { (x^2+y-11)^2 + (x+y^2-7)^2 }

  # partial derivatives
  dx <- function(x,y) {4*x**3-4*x*y-42*x+4*x*y-14}
  dy <- function(x,y) {4*y**3+2*x**2-26*y+4*x*y-22}

  # gradient descent parameters
  num_iter <- 240
  learning_rate <- 0.0005
  x_val <- 6
  y_val <- 6

  # momentum parameters
  beta = 0.95
  vdx = 0
  vdy = 0

  updates_x <- vector("numeric", length = num_iter)
  updates_y <- vector("numeric", length = num_iter)
  updates_z <- vector("numeric", length = num_iter)

  # parameter updates
  for (i in 1:num_iter) {

    vdx = beta*vdx+dx(x_val,y_val)
    vdy = beta*vdy+dy(x_val,y_val)

    x_val <- x_val-learning_rate*vdx
    y_val <- y_val-learning_rate*vdy
    z_val <- f(x_val, y_val)

    updates_x[i] <- x_val
    updates_y[i] <- y_val
    updates_z[i] <- z_val
  }

  # plot
  z <- outer(x, y, f)

  video <- av_capture_graphics(output = output,  audio = big_horns_intro, {
    for(i in seq_len(len)){
      if(as.logical(verbose))
        cat(sprintf("\rGenerating plot %d/%d...", i, len), file = stderr())


      plt <- persp(x, y, z,
                   theta = -50-i*0.2, phi = 20+log(i),
                   expand = 0.5,
                   col = "lightblue", border = 'lightblue',
                   axes = FALSE, box = FALSE,
                   ltheta = 60, shade = 0.90
      )

      points(trans3d(updates_x[1:i], updates_y[1:i], updates_z[1:i], pmat = plt), pch = 16,
             cex = c(rep(0.6, i-1), 1.2),
             col = c(rep('white', i-1), 'black')
      )

    }
    if(as.logical(verbose))
      cat("done!\n", file = stderr())
  }, width = width, height = height, res = res, framerate = framerate, verbose = verbose, ...)
  if(interactive())
    utils::browseURL(video)
  return(video)
}
