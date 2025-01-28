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
png_path <- file.path(tempdir(), "frame%03d.png")
png_files <- sprintf(png_path, 1:n)
wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av', mustWork = TRUE)

# If libx264 is available, it is usually the default.
# Note that it is not available for libavfilter-free-devel in Fedora
has_libx264 <- "libx264" %in% av_muxers()$video

test_that("convert images into video formats", {
  png(png_path, width = width, height = height, res = 72 * q)
  par(ask = FALSE)
  for(i in 1:n){
    hist(rnorm(100, mean = i), main = paste("histogram with mean", i))
  }
  dev.off()
  for(ext in c("mkv", "mp4", "mov", "flv", "gif")){
    filename <- paste0("test.", ext)
    av::av_encode_video(png_files, filename, framerate = framerate, verbose = FALSE)
    expect_true(file.exists(filename))
    info <- av_media_info(filename)

    expect_equal(info$video$width, width)
    expect_equal(info$video$height, height)
    if(has_libx264){
      expect_equal(info$video$codec, switch(ext, gif='gif', flv='flv', 'h264'))
    }
    expect_equal(info$video$framerate, framerate)

    # Gif doesn't have video metadata (duration)
    if(ext != 'gif'){
      expect_equal(info$duration, n / framerate)

      # Test converting video to video
      av_video_convert(filename, 'test2.mp4', verbose = FALSE)
      info <- av_media_info('test2.mp4')
      unlink('test2.mp4')
      expect_equal(info$video$width, width)
      expect_equal(info$video$height, height)
      expect_equal(info$video$framerate, framerate)
      expect_null(info$audio)
    }
    unlink(filename)
  }
})

test_that("audio sampling works", {
  for(ext in c("mkv", "mp4", "mov", "flv")){
    filename <- paste0("test.", ext)
    if(ext == 'mkv' && grepl("(Fedora|Red Hat)", osVersion)){
      # libavfilter-free-devel only supports certain audio bitrates
      wonderland <- av_audio_convert(wonderland, tempfile(fileext = '.mka'), sample_rate = 48000, verbose = FALSE)
    }
    input_rate <- av_media_info(wonderland)$audio$sample_rate
    av::av_encode_video(png_files, filename, framerate = framerate, verbose = FALSE, audio = wonderland)
    expect_true(file.exists(filename))
    info <- av_media_info(filename)

    expect_equal(info$video$width, width)
    expect_equal(info$video$height, height)
    if(has_libx264){
      expect_equal(info$video$codec, switch(ext, flv='flv', 'h264'))
    }
    expect_equal(info$video$framerate, framerate)

    expect_equal(info$audio$channels, 2)
    expect_equal(info$audio$sample_rate, input_rate)

    # Audio stream may slightly alter the duration, 5% margin
    expect_equal(info$duration, n / framerate, tolerance = 0.05)

    # Test converting video to video
    av_video_convert(filename, 'test2.mp4', verbose = FALSE)
    info <- av_media_info('test2.mp4')
    unlink('test2.mp4')
    expect_equal(info$video$width, width)
    expect_equal(info$video$height, height)
    expect_equal(info$video$framerate, framerate)

    expect_equal(info$audio$channels, 2)
    expect_equal(info$audio$sample_rate, input_rate)

    # Audio stream may slightly alter the duration, 5% margin
    expect_equal(info$duration, n / framerate, tolerance =  0.05)

    # Cleanup
    unlink(filename)
  }
})

test_that("fractional framerates work", {
  framerate <- 1/5
  av::av_encode_video(png_files, 'test.mp4', framerate = framerate, verbose = FALSE)
  info <- av_media_info('test.mp4')
  unlink('test.mp4')

  expect_equal(info$video$framerate, framerate)
  expect_equal(info$duration, length(png_files) / framerate)

})

test_that("speed up/down filters", {
  for (x in c(0.1, 0.5, 2, 10)){
    framerate <- 25
    filter <- sprintf("setpts=%s*PTS", as.character(x))
    av::av_encode_video(png_files, 'speed.mp4', framerate = framerate, vfilter = filter, verbose = FALSE)
    info <- av_media_info('speed.mp4')
    unlink('speed.mp4')
    expect_equal(info$video$framerate, (framerate / x))
    expect_equal(info$duration, length(png_files) / (framerate / x))
  }
})

test_that("test error handling", {
  wrongfile <- system.file('DESCRIPTION', package='av')
  file.copy(wrongfile, tmp <- tempfile(fileext = '.png'))
  unlink("video.mp4")
  expect_error(av_encode_video(wrongfile, verbose = FALSE), "input")
  expect_false(file.exists("video.mp4"))
  expect_error(av_encode_video(tmp, verbose = -8), "input")
  expect_false(file.exists("video.mp4"))
  expect_error(av_encode_video(png_files, vfilter = "doesnontexist", verbose = -8), "filter")
  expect_false(file.exists("video.mp4"))

  # Test missing stream
  expect_error(av_encode_video(png_files, audio = png_files[1]), "audio")
  expect_false(file.exists("video.mp4"))
  expect_error(av_encode_video(wonderland), "video")
  expect_false(file.exists("video.mp4"))

  # Test time limit
  setTimeLimit(cpu = 2, transient = TRUE)
  timings <- system.time(expect_error(av_encode_video(rep(png_files, 100), verbose = FALSE), "time"))
  setTimeLimit()
  expect_lt(timings[['sys.self']], 2.5)
  expect_equal(get_open_handles(), 0)
})
