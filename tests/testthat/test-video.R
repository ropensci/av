context("Convert Video")

test_that("Set log level", {
  av_log_level(16)
  expect_equal(av_log_level(), 16)
})

q <- 1
n <- 50
framerate <- 10
width <- 640 * q
height <- 480 * q
png_files <- NULL

test_that("convert images into video formats", {
  png_path <- file.path(tempdir(), "frame%03d.png")
  png(png_path, width = width, height = height, res = 72 * q)
  par(ask = FALSE)
  for(i in 1:n){
    hist(rnorm(100, mean = i), main = paste("histogram with mean", i))
  }
  dev.off()
  png_files <<- sprintf(png_path, 1:n)
  for(ext in c("mkv", "mp4", "mov", "flv", "gif")){
    filename <- paste0("test.", ext)
    av::av_encode_video(png_files, filename, framerate = framerate, verbose = FALSE)
    expect_true(file.exists(filename))
    info <- av_video_info(filename)
    unlink(filename)
    expect_equal(info$video$width, width)
    expect_equal(info$video$height, height)
    expect_equal(info$video$codec, switch(ext, gif='gif', flv='flv', 'h264'))
    expect_equal(info$video$framerate, framerate)

    # Gif doesn't have video metadata (duration)
    if(ext != 'gif')
      expect_equal(info$duration, (n-1) / framerate)
  }
})

test_that("audio sampling works", {
  wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av', mustWork = TRUE)
  for(ext in c("mkv", "mp4", "mov", "flv")){
    filename <- paste0("test.", ext)
    av::av_encode_video(png_files, filename, framerate = framerate, verbose = FALSE, audio = wonderland)
    expect_true(file.exists(filename))
    info <- av_video_info(filename)
    unlink(filename)
    expect_equal(info$video$width, width)
    expect_equal(info$video$height, height)
    expect_equal(info$video$codec, switch(ext, gif='gif', flv='flv', 'h264'))
    expect_equal(info$video$framerate, framerate)

    expect_equal(info$audio$channels, 2)
    expect_equal(info$audio$sample_rate, 44100)
    expect_equal(substring(info$audio$codec, 1, 3), switch(ext, mkv='ac3', flv='mp3', 'aac'))

    # Audio stream may slightly alter the duration, 1 sec margin
    expect_gte(info$duration, (n-1) / framerate)
    expect_lt(info$duration - 1, (n-1) / framerate)
  }
})

test_that("fractional framerates work", {
  framerate <- 1/5
  av::av_encode_video(png_files, 'test.mp4', framerate = framerate, verbose = FALSE)
  info <- av_video_info('test.mp4')
  unlink('test.mp4')

  expect_equal(info$video$framerate, framerate)
  expect_equal(info$duration, (length(png_files)-1) / framerate)

})

test_that("speed up/down filters", {
  for (x in c(0.1, 0.5, 2, 10)){
    framerate <- 25
    filter <- sprintf("setpts=%s*PTS", as.character(x))
    av::av_encode_video(png_files, 'speed.mp4', framerate = framerate, filter = filter, verbose = FALSE)
    info <- av_video_info('speed.mp4')
    unlink('speed.mp4')
    expect_equal(info$video$framerate, (framerate / x))
    expect_equal(info$duration, (length(png_files)-1) / (framerate / x))
  }
})
