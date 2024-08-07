library(testthat)
library(av)

if (0 && ps::ps_is_supported()) {
  # This sometimes has false positives on MacOS https://github.com/r-lib/ps/issues/90
  reporter <- ps::CleanupReporter(testthat::CheckReporter)$new(proc_cleanup = FALSE, proc_fail = FALSE, conn_fail = FALSE)
  test_check("av", reporter = reporter)
} else {
  test_check("av")
}
