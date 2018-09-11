library(testthat)
library(av)

if (ps::ps_is_supported()) {
  reporter <- ps::CleanupReporter(testthat::CheckReporter)$new(proc_cleanup =  FALSE,  proc_fail = FALSE)
  test_check("av", reporter = reporter)
} else {
  test_check("av")
}
