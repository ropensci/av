#' AV Formats
#'
#' List supported codecs and container formats.
#'
#'
#' @export
#' @rdname formats
#' @family av
av_encoders <- function(){
  list_codecs(TRUE)
}

#' @export
#' @rdname formats
av_decoders <- function(){
  list_codecs(FALSE)
}

#' @useDynLib av R_list_codecs
list_codecs <- function(is_encoder){
  res <- as.list(.Call(R_list_codecs))
  names(res) <- c("type", "encoder", "name", "description")
  df <- data.frame(res, stringsAsFactors = FALSE)
  df <- df[df$encoder == is_encoder, c("type", "name", "description")]
  df <- df[order(df$name),]
  row.names(df) <- NULL
  df
}
