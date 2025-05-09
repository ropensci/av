% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/capture.R
\name{capturing}
\alias{capturing}
\alias{av_capture_graphics}
\alias{av_spectrogram_video}
\title{Record Video from Graphics Device}
\usage{
av_capture_graphics(
  expr,
  output = "output.mp4",
  width = 720,
  height = 480,
  framerate = 1,
  vfilter = "null",
  audio = NULL,
  verbose = TRUE,
  ...
)

av_spectrogram_video(
  audio,
  output = "output.mp4",
  framerate = 25,
  verbose = interactive(),
  ...
)
}
\arguments{
\item{expr}{an R expression that generates the graphics to capture}

\item{output}{name of the output file. File extension must correspond to a known
container format such as \code{mp4}, \code{mkv}, \code{mov}, or \code{flv}.}

\item{width}{width in pixels of the graphics device}

\item{height}{height in pixels of the graphics device}

\item{framerate}{video framerate in frames per seconds. This is the input fps, the
output fps may be different if you specify a filter that modifies speed or interpolates
frames.}

\item{vfilter}{a string defining an ffmpeg filter graph. This is the same parameter
as the \code{-vf} argument in the \code{ffmpeg} command line utility.}

\item{audio}{path to media file with audio stream}

\item{verbose}{emit some output and a progress meter counting processed images. Must
be \code{TRUE} or \code{FALSE} or an integer with a valid \link{av_log_level}.}

\item{...}{extra graphics parameters passed to \code{\link[=png]{png()}}}
}
\description{
Runs the expression and captures all plots into a video. The \link{av_spectrogram_video}
function is a wrapper that plots data from \link{read_audio_fft} with a moving bar and
background audio.
}
\examples{
\donttest{
library(gapminder)
library(ggplot2)
makeplot <- function(){
  datalist <- split(gapminder, gapminder$year)
  lapply(datalist, function(data){
    p <- ggplot(data, aes(gdpPercap, lifeExp, size = pop, color = continent)) +
      scale_size("population", limits = range(gapminder$pop)) + geom_point() + ylim(20, 90) +
      scale_x_log10(limits = range(gapminder$gdpPercap)) + ggtitle(data$year) + theme_classic()
    print(p)
  })
}

# Play 1 plot per sec, and use an interpolation filter to convert into 10 fps
video_file <- file.path(tempdir(), 'output.mp4')
av_capture_graphics(makeplot(), video_file, 1280, 720, res = 144, vfilter = 'framerate=fps=10')
av::av_media_info(video_file)
# utils::browseURL(video_file)}
}
\seealso{
Other av: 
\code{\link{demo}()},
\code{\link{encoding}},
\code{\link{formats}},
\code{\link{info}},
\code{\link{logging}},
\code{\link{read_audio_fft}()}
}
\concept{av}
