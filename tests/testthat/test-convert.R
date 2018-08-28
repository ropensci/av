context("Convert Video")

test_that("Set log level", {
  av_log_level(16)
  expect_equal(av_log_level(), 16)
})

test_that("mp4 to mkv", {
  input <- file.path(tempdir(), 'input.mp4')
  output <- file.path(tempdir(), 'output.mkv')
  curl::curl_download('http://techslides.com/demos/sample-videos/small.mp4', 
                input, quiet = TRUE)
  av_convert(input, output)
  expect_true(file.exists(output))
})
