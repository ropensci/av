# av

> R Bindings to FFmpeg

[![Project Status: Active – The project has reached a stable, usable state and is being actively developed.](https://www.repostatus.org/badges/latest/active.svg)](https://www.repostatus.org/#active)
[![Build Status](https://travis-ci.org/ropensci/av.svg?branch=master)](https://travis-ci.org/ropensci/av)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/ropensci/av?branch=master)](https://ci.appveyor.com/project/jeroen/av)
[![CRAN_Status_Badge](http://www.r-pkg.org/badges/version/av)](https://cran.r-project.org/package=av)

## Installation

You can install `av` from CRAN

```r
install.packages("av")
```

On Debian/Ubuntu you first need to install [libavfilter-dev](https://packages.debian.org/bullseye/libavfilter-dev)

```
sudo apt-get install libavfilter-dev
```

And on Fedora / CentOS / RHEL you need to install `ffmpeg-devel` from [rpmfusion](https://rpmfusion.org/Configuration). See [instructions here](https://rpmfusion.org/Configuration#Command_Line_Setup_using_rpm) on how to enable rpmfusion via the command line.

```
# Need to enable rpmfusion repository first via link above!
sudo yum install ffmpeg-devel
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
# Create the gganimate plot
library(gganimate)
library(transformr)
p <- ggplot(airquality, aes(Day, Temp)) + 
  geom_line(size = 2, colour = 'steelblue') + 
  transition_states(Month, 4, 1) + 
  shadow_mark(size = 1, colour = 'grey')

# Render and show the video
q <- 2
df <- animate(p, renderer = av_renderer('animation.mp4'), width = 720*q, height = 480*q, res = 72*q, fps = 25)
utils::browseURL('animation.mp4')
```

## Video Filters

You can add a custom [ffmpeg video filter chain](https://ffmpeg.org/ffmpeg-filters.html#Video-Filters). For example this will negate the colors, and applies an orange fade-in effect to the first 15 frames.

```r
# Continue on the example above
filter_render <- av_renderer('orange.mp4', vfilter = 'negate=1, fade=in:0:15:color=orange')
df <- animate(p, renderer = filter_render, width = 720*q, height = 480*q, res = 72*q, fps = 25)
av::av_media_info('orange.mp4')
utils::browseURL('orange.mp4')
```

Filters can also affect the final fps of the video. For example this filter will double fps because it halves presentation the timestamp (pts) of each frame. Hence the output framerate is actually 50!

```r
fast_render <- av_renderer('fast.mp4', vfilter = "setpts=0.5*PTS")
df <- animate(p, renderer = fast_render, fps = 25)
av::av_media_info('fast.mp4')
utils::browseURL('fast.mp4')
```

## Capture Graphics (without gganimate)

Instead of using gganimate, we can use `av_capture_graphics()` to automatically record R graphics and turn them into a video. This example makes 12 plots and adds an interpolation filter to smoothen the transitions between the frames.

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
av::av_media_info(video_file)
utils::browseURL(video_file)
```
