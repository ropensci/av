context("Convert Audio")

wonderland <- system.file('samples/Synapsis-Wonderland.mp3', package='av', mustWork = TRUE)

test_that("Audio can be converted to various formats", {
  tmp_wav <- normalizePath(tempfile(fileext = '.wav'), mustWork = FALSE)
  tmp_mp3 <- normalizePath(tempfile(fileext = '.mp3'), mustWork = FALSE)
  tmp_mp4 <- normalizePath(tempfile(fileext = '.mp4'), mustWork = FALSE)
  tmp_mkv <- normalizePath(tempfile(fileext = '.mkv'), mustWork = FALSE)
  input_info <- av_media_info(wonderland)

  # Convert mp3 to mkv (defaults to libvorbis audio)
  expect_equal(av_audio_convert(wonderland, tmp_mkv, verbose = FALSE), tmp_mkv)
  expect_true(file.exists(tmp_mkv))
  mkv_info <- av_media_info(tmp_mkv)
  expect_equal(input_info$duration, mkv_info$duration, tolerance = 0.1)
  expect_equal(nrow(mkv_info$audio), 1)

  # Convert mp3 to mp4 (defaults to aac audio)
  expect_equal(av_audio_convert(wonderland, tmp_mp4, verbose = FALSE), tmp_mp4)
  expect_true(file.exists(tmp_mp4))
  mp4_info <- av_media_info(tmp_mp4)
  expect_equal(input_info$duration, mp4_info$duration, tolerance = 0.1)
  expect_equal(nrow(mp4_info$audio), 1)

  # Convert mp3 to wav
  expect_equal(av_audio_convert(wonderland, tmp_wav, verbose = FALSE), tmp_wav)
  expect_true(file.exists(tmp_wav))
  wav_info <- av_media_info(tmp_wav)
  expect_equal(input_info$duration, wav_info$duration, tolerance = 0.1)
  expect_equal(nrow(wav_info$audio), 1)

  # Convert wav to mp3
  expect_equal(av_audio_convert(tmp_wav, tmp_mp3, verbose = FALSE), tmp_mp3)
  expect_true(file.exists(tmp_mp3))
  mp3_info <- av_media_info(tmp_mp3)
  expect_equal(input_info$duration, mp3_info$duration, tolerance = 0.1)
  expect_equal(nrow(mp3_info$audio), 1)

})

test_that("Audio truncation works as expected",{
  for(start_time in c(0, 5, 20)){
    tmp_mp3 <- normalizePath(tempfile(fileext = '.mp3'), mustWork = FALSE)
    av_audio_convert(wonderland, tmp_mp3, start_time = start_time, total_time = 10, verbose = FALSE)
    info <- av_media_info(tmp_mp3)
    expect_equal(info$duration, 10, tolerance = 0.05)
  }
})
