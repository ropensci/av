context("Convert Video")

test_that("Set log level", {
  av_log_level(16)
  expect_equal(av_log_level(), 16)
})

test_that("convert images into video formats", {
  q <- 2
  n <- 50
  png_path <- file.path(tempdir(), "frame%03d.png")
  png(png_path, width = 640 * q, height = 480 * q, res = 72 * q)
  par(ask = FALSE)
  for(i in 1:n){
    hist(rnorm(100, mean = i), main = paste("histogram with mean", i))
  }
  dev.off()
  png_files <- sprintf(png_path, 1:n)
  types <- c("mkv", "mp4", "flv", "mov")
  for(ext in types){
    filename <- paste0("test.", ext)
    av::create_video(png_files, filename, fps = 5)
    expect_true(file.exists(filename))
    unlink(filename)
  }
})
