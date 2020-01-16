context("Audio FFT")

test_that("Audio FFT", {
  wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av')
  extensions <- c("wav", "mkv", "ac3", "flac")
  av_log_level(16) # muffle some warnings about ac3 vbr
  for(ext in extensions){
    filename <- paste0('wonderland.', ext)
    av_audio_convert(wonderland, filename, verbose = FALSE)
    data <- read_audio_fft(filename, window = hanning(2048))
    expect_equal(dim(data)[1], 1024)
    expect_equal(dim(data)[2], 2584, tol = 0.001)
    unlink(filename)
  }
})
