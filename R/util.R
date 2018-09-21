#' @useDynLib av R_new_ptr
new_ptr <- function(){
  .Call(R_new_ptr)
}

#' @useDynLib av R_run_cleanup
run_cleanup <- function(ptr){
  .Call(R_run_cleanup, ptr)
}
