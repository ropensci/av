# av

> R Bindings to FFmpeg

[![Project Status: Active – The project has reached a stable, usable state and is being actively developed.](http://www.repostatus.org/badges/latest/wip.svg)](http://www.repostatus.org/#wip)
[![Build Status](https://travis-ci.org/jeroen/av.svg?branch=master)](https://travis-ci.org/jeroen/av)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/jeroen/av?branch=master)](https://ci.appveyor.com/project/jeroen/av)

## Installation

You can install the development version from [GitHub](https://github.com/jeroen/av) with:

```r
# Install from GH
devtools::install_github("jeroen/av")

# For the demo
devtools::install_github("thomasp85/gganimate")
```

## Hello World

Example using gganimate:

```r

# Define the "renderer" for gganimate
av_renderer <- function(filter = "null", filename = 'output.mp4'){
  function(frames, fps){
    unlink(filename)
    av::av_encode_video(frames, filename, framerate = fps, filter = filter)
  }
}

# Create the gganimate plot
library(gganimate)
p <- ggplot(airquality, aes(Day, Temp)) + 
  geom_line(size = 2, colour = 'steelblue') + 
  transition_states(Month, 4, 1) + 
  shadow_mark(size = 1, colour = 'grey')

# Render and show the video
q <- 2
df <- animate(p, renderer = av_renderer(), width = 640*q, height = 480*q, res = 72*q, fps = 25)
utils::browseURL('output.mp4')
```
