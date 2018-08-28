library(testthat)
library(av)

download.file("http://www.google.com", tempfile(), quiet = TRUE)
reporter <- if (ps::ps_is_supported()) {
  ps::CleanupReporter(testthat::SummaryReporter)$new(proc_cleanup =  FALSE,  proc_fail = FALSE)
} else {
  "summary"
}

test_check("av", reporter = reporter)
