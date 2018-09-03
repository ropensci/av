context("Convert Video")

test_that("Set log level", {
  av_log_level(16)
  expect_equal(av_log_level(), 16)
})

test_that("convert images into video formats", {
  q <- 2
  n <- 50
  framerate <- 5
  width <- 640 * q
  height <- 480 * q
  png_path <- file.path(tempdir(), "frame%03d.png")
  png(png_path, width = width, height = height, res = 72 * q)
  par(ask = FALSE)
  for(i in 1:n){
    hist(rnorm(100, mean = i), main = paste("histogram with mean", i))
  }
  dev.off()
  png_files <- sprintf(png_path, 1:n)
  types <- c("mkv", "mp4", "mov") #todo: add flv
  for(ext in types){
    filename <- paste0("test.", ext)
    av::create_video(png_files, filename, framerate = framerate)
    expect_true(file.exists(filename))
    info <- av_video_info(filename)
    expect_equal(info$duration, n / framerate)
    expect_equal(info$video_streams$width, width)
    expect_equal(info$video_streams$height, height)
    expect_equal(info$video_streams$codec, "h264")
    expect_equal(info$video_streams$framerate, framerate)
    unlink(filename)
  }
})
