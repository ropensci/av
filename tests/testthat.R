library(testthat)
library(av)

reporter <- if (ps::ps_is_supported()) {
  ps::CleanupReporter(testthat::SummaryReporter)$new(proc_cleanup =  FALSE,  proc_fail = FALSE)
} else {
  "summary"
}

test_check("av", reporter = reporter)
