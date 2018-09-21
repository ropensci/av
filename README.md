# av

> R Bindings to FFmpeg

[![Project Status: Active – The project has reached a stable, usable state and is being actively developed.](http://www.repostatus.org/badges/latest/wip.svg)](http://www.repostatus.org/#wip)
[![Build Status](https://travis-ci.org/ropensci/av.svg?branch=master)](https://travis-ci.org/ropensci/av)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/ropensci/av?branch=master)](https://ci.appveyor.com/project/jeroen/av)

## Installation

You can install the development version from [GitHub](https://github.com/ropensci/av) with:

```r
# Install from GH
devtools::install_github("ropensci/av")

# For the demo
devtools::install_github("thomasp85/gganimate")
```

## Demo Video

Generate a demo video with some random plots and free [demo music](https://freemusicarchive.org/music/Synapsis/~/Wonderland):

```r
av::av_demo()
```

This demo is totally lame, please open a PR with something better (in base R!).

## Using gganimate

You can use `av_encode_video()` as the renderer in gganimate:

```r
# Get latest gganimate
# devtools::install_github("thomasp85/gganimate")
library(gganimate)

# Define the "renderer" for gganimate
av_renderer <- function(vfilter = "null", filename = 'output.mp4'){
  function(frames, fps){
    unlink(filename)
    av::av_encode_video(frames, filename, framerate = fps, vfilter = vfilter)
  }
}

# Create the gganimate plot
p <- ggplot(airquality, aes(Day, Temp)) + 
  geom_line(size = 2, colour = 'steelblue') + 
  transition_states(Month, 4, 1) + 
  shadow_mark(size = 1, colour = 'grey')

# Render and show the video
q <- 2
df <- animate(p, renderer = av_renderer(), width = 720*q, height = 480*q, res = 72*q, fps = 25)
utils::browseURL('output.mp4')
```

## Video Filters

You can add a custom [ffmpeg video filter chain](https://ffmpeg.org/ffmpeg-filters.html#Video-Filters). For example this will negate the colors, and applies an orange fade-in effect to the first 15 frames.

```r
# Continue on the example above
myrenderer <- av_renderer(vfilter = 'negate=1, fade=in:0:15:color=orange')
df <- animate(p, renderer = myrenderer, width = 720*q, height = 480*q, res = 72*q, fps = 25)
av::av_video_info('output.mp4')
utils::browseURL('output.mp4')
```

Filters can also affect the final fps of the video. For example this filter will double fps because it halves presentation the timestamp (pts) of each frame. Hence the output framerate is actually 20!

```r
myrenderer <- av_renderer(vfilter = "setpts=0.5*PTS")
df <- animate(p, renderer = myrenderer, fps = 10)
av::av_video_info('output.mp4')
utils::browseURL('output.mp4')
```

## Capture Graphics

Beside gganimate, we can use `av_capture_graphics()` to automatically record R graphics and turn them into a video. This example makes 12 plots and adds an interpolation filter to smoothen the transitions between the frames.

```r
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
av::av_capture_graphics(makeplot(), video_file, 1280, 720, res = 144, vfilter = 'framerate=fps=10')
av::av_video_info(video_file)
utils::browseURL(video_file)
```
