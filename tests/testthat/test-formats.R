context("List Formats")

test_that("Listing formats", {
  encoders <- av_encoders()
  decoders <- av_decoders()
  expect_is(encoders, 'data.frame')
  expect_is(decoders, 'data.frame')
  expect_gt(nrow(encoders), 100)
  expect_gt(nrow(decoders), 400)
  expect_equal(names(encoders), c("type", "name", "description"))
  expect_equal(names(decoders), c("type", "name", "description"))
})
