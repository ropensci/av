#' Input Devices
#'
#' Recording audio / video from input devices such as camera, microphone, or
#' screen capture.
#'
#' @export
#' @useDynLib av R_list_input_devices
av_input_devices <- function(){
  res <-as.list(.Call(R_list_input_devices))
  names(res) <- c("name", "type", "description")
  res$type <- ifelse(res$type, "video", "audio")
  data.frame(res, stringsAsFactors = FALSE)
}
