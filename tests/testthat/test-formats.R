context("List Formats")

test_that("Listing formats", {
  encoders <- av_encoders()
  decoders <- av_decoders()
  filters <- av_filters()
  muxers <- av_muxers()
  demuxers <- av_demuxers()
  expect_is(encoders, 'data.frame')
  expect_is(decoders, 'data.frame')
  expect_is(filters, 'data.frame')
  expect_is(muxers, 'data.frame')
  expect_is(demuxers, 'data.frame')
  expect_gt(nrow(encoders), 100)
  expect_gt(nrow(decoders), 400)
  expect_gt(nrow(filters), 300)
  expect_gt(nrow(muxers), 100)
  expect_gt(nrow(demuxers), 100)
  expect_equal(names(encoders), c("type", "name", "description", "format"))
  expect_equal(names(decoders), c("type", "name", "description", "format"))
  expect_equal(names(filters), c("name", "description"))
})

test_that("Critical encoders", {
  encoders <- av_encoders()
  expect_contains(encoders$name, 'libmp3lame')
  expect_contains(encoders$name, 'libvorbis')
  expect_contains(encoders$name, 'libxvid')
  if(!grepl("(Fedora|Rocky|Red Hat)", osVersion)){
    # libx264 is not supported on RHEL/Fedora libavfilter-free-devel but it is
    # in ffmpeg-devel from rpmfusion.
    expect_contains(encoders$name, 'libx264')
  }
})
