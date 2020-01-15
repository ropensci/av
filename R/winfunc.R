#' Window functions
#'
#' Several common [windows function](https://en.wikipedia.org/wiki/Window_function)
#' generators. The functions return a vector of weights to use in [read_audio_fft].
#'
#' @export
#' @name window functions
#' @rdname winfunc
#' @param n size of the window (number of weights to generate)
#' @examples # Window functions
#' plot(hanning(1024), type = 'l', xlab = 'window', ylab = 'weight')
#' lines(hamming(1024), type = 'l', col = 'red')
#' lines(bartlett(1024), type = 'l', col = 'blue')
#' lines(welch(1024), type = 'l', col = 'purple')
#' lines(flattop(1024), type = 'l', col = 'darkgreen')
hanning <- function(n){
  generate_window(n, 1L)
}

#' @export
#' @rdname winfunc
hamming <- function(n){
  generate_window(n, 2L)
}

#' @export
#' @rdname winfunc
blackman <- function(n){
  generate_window(n, 3L)
}

#' @export
#' @rdname winfunc
bartlett <- function(n){
  generate_window(n, 4L)
}

#' @export
#' @rdname winfunc
welch <- function(n){
  generate_window(n, 5L)
}

#' @export
#' @rdname winfunc
flattop <- function(n){
  generate_window(n, 6L)
}

#' @export
#' @rdname winfunc
bharris <- function(n){
  generate_window(n, 7L)
}

#' @export
#' @rdname winfunc
bnuttall <- function(n){
  generate_window(n, 8L)
}

#' @export
#' @rdname winfunc
sine <- function(n){
  generate_window(n, 9L)
}

#' @export
#' @rdname winfunc
nuttall <- function(n){
  generate_window(n, 10L)
}

#' @export
#' @rdname winfunc
bhann <- function(n){
  generate_window(n, 11L)
}

#' @export
#' @rdname winfunc
lanczos <- function(n){
  generate_window(n, 12L)
}

#' @export
#' @rdname winfunc
gauss <- function(n){
  generate_window(n, 13L)
}

#' @export
#' @rdname winfunc
tukey <- function(n){
  generate_window(n, 14L)
}

#' @export
#' @rdname winfunc
dolph <- function(n){
  generate_window(n, 15L)
}

#' @export
#' @rdname winfunc
cauchy <- function(n){
  generate_window(n, 16L)
}

#' @export
#' @rdname winfunc
parzen <- function(n){
  generate_window(n, 17L)
}

#' @export
#' @rdname winfunc
bohman <- function(n){
  generate_window(n, 19L)
}

#' @useDynLib av R_generate_window
generate_window <- function(n, type){
  n <- as.integer(n)
  type <- as.integer(type)
  assert_range(n)
  assert_range(type, max = 19)
  .Call(R_generate_window, n, type)
}

assert_range <- function(x, min = 0, max = Inf){
  stopifnot(length(x) == 1)
  stopifnot(x >= min)
  stopifnot(x <= max)
}
