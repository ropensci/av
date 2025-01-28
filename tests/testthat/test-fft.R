context("Read raw audio")

wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av')

test_that("Audio FFT", {
  extensions <- c("wav", "mkv", "ac3", "flac")
  av_log_level(16) # muffle some warnings about ac3 vbr
  for(ext in extensions){
    filename <- paste0('wonderland.', ext)
    # libopus on fedora does not do support input sample_rate 44100
    av_audio_convert(wonderland, filename, verbose = FALSE, sample_rate = 48000)
    data <- read_audio_fft(filename, window = hanning(2048))
    expect_equal(dim(data)[1], 1024)
    expect_equal(dim(data)[2], 2813, tol = 0.001)
    unlink(filename)
  }
})

test_that("FFT time cutting",{
  fft_orig <- read_audio_fft(wonderland)
  fft0 <- read_audio_fft(wonderland, start_time = 0, end_time = 10)
  fft1 <- read_audio_fft(wonderland, start_time = 10, end_time = 20)
  fft2 <- read_audio_fft(wonderland, start_time = 10, end_time = 100)

  expect_equal(ncol(fft0), ncol(fft_orig)/3, tol = 0.01)
  expect_equal(ncol(fft1), ncol(fft_orig)/3, tol = 0.01)
  expect_equal(ncol(fft2), ncol(fft_orig)*2/3, tol = 0.01)

})

test_that("Read binary audio", {
  out1 <- read_audio_bin(wonderland)
  out2 <- read_audio_bin_old(wonderland)
  expect_identical(out1, out2)

  out1 <- read_audio_bin(wonderland, channels = 1)
  out2 <- read_audio_bin_old(wonderland, channels = 1)
  expect_identical(out1, out2)

  out1_short <- read_audio_bin(wonderland, start_time = 5)
  out2_short <- read_audio_bin_old(wonderland, start_time = 5)
  expect_identical(out1_short, out2_short)
})

test_that("Write binary audio", {
  bin <- read_audio_bin(wonderland)
  info <- av_media_info(wonderland)
  channels <- info$audio$channels
  bitrate <- info$audio$bitrate
  output <- tempfile(fileext = '.mp3')
  write_audio_bin(bin, pcm_channels = channels, bit_rate = bitrate, output = output, verbose=FALSE)
  info2 <- av_media_info(output)
  unlink(output)
  expect_equal(info, info2, tolerance = 0.0001)
})
