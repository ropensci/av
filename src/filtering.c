#include <libavutil/opt.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

void bail_if(int ret, const char * what);

AVFrame *filter_single_frame(AVFrame * input, enum AVPixelFormat fmt, int width, int height){
  
  /* Create a new filter graph */
  AVFilterGraph *filter_graph = avfilter_graph_alloc();
  
  /* Initiate source filter */
  char input_args[512];
  snprintf(input_args, sizeof(input_args),
           "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
           input->width, input->height, input->format, 1, 1,
           input->sample_aspect_ratio.num, input->sample_aspect_ratio.den);
  AVFilterContext *buffersrc_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("buffer"), "in", 
                                       input_args, NULL, filter_graph), "avfilter_graph_create_filter (input_args)");
  
  /* Initiate sink filter */
  AVFilterContext *buffersink_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("buffersink"), "out",
                               NULL, NULL, filter_graph), "avfilter_graph_create_filter (output)");
  
  /* I think this convert output YUV420P (copied from ffmpeg examples/transcoding.c) */
  bail_if(av_opt_set_bin(buffersink_ctx, "pix_fmts",
                       (uint8_t*)&fmt, sizeof(fmt),
                       AV_OPT_SEARCH_CHILDREN), "av_opt_set_bin");
  
  /* Adding scale filter */
  char scale_args[512];
  snprintf(scale_args, sizeof(scale_args), "%d:%d", width, height);
  AVFilterContext *scale_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&scale_ctx, avfilter_get_by_name("scale"), 
                                       "scale", scale_args, NULL, filter_graph), "avfilter_graph_create_filter");
  
  /* Link the filter sequence */
  bail_if(avfilter_link(buffersrc_ctx, 0, scale_ctx, 0), "avfilter_link 1");
  bail_if(avfilter_link(scale_ctx, 0, buffersink_ctx, 0), "avfilter_link 2");
  bail_if(avfilter_graph_config(filter_graph, NULL), "avfilter_graph_config");
  
  /* Insert the frame into the source filter */
  bail_if(av_buffersrc_add_frame_flags(buffersrc_ctx, input, 0), "av_buffersrc_add_frame_flags");
  
  /* Extract output frame from the sink filter */
  AVFrame * output = av_frame_alloc();
  bail_if(av_buffersink_get_frame(buffersink_ctx, output), "av_buffersink_get_frame");
  av_frame_free(&input);
  
  /* Return output frame */
  avfilter_free(buffersrc_ctx);
  avfilter_free(scale_ctx);
  avfilter_free(buffersink_ctx);
  avfilter_graph_free(&filter_graph);
  
  /* Return an output frame */
  output->pict_type = AV_PICTURE_TYPE_I;
  return output;
}
