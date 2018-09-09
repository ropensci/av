#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

#define VIDEO_TIME_BASE 1000

typedef struct {
  AVFormatContext *container;
  AVCodecContext *video_encoder;
  AVStream *video_stream;
} output_container;

typedef struct {
  AVFilterContext *input;
  AVFilterContext *output;
  AVFilterGraph *graph;
} video_filter;

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(void * ptr, const char * what){
  if(!ptr)
    bail_if(-1, what);
}

static output_container *open_output_file(const char *filename, int width, int height, AVCodec *codec, int len){
  /* Init container context (infers format from file extension) */
  AVFormatContext *container = NULL;
  avformat_alloc_output_context2(&container, NULL, NULL, filename);
  bail_if_null(container, "avformat_alloc_output_context2");

  /* Init video encoder */
  AVCodecContext *video_encoder = avcodec_alloc_context3(codec);
  bail_if_null(video_encoder, "avcodec_alloc_context3");
  video_encoder->height = height;
  video_encoder->width = width;
  video_encoder->time_base.num = 1;
  video_encoder->time_base.den = VIDEO_TIME_BASE;
  video_encoder->framerate = av_inv_q(video_encoder->time_base);
  video_encoder->gop_size = 5;
  video_encoder->max_b_frames = 1;

  /* Try to use codec preferred pixel format, otherwise default to YUV420 */
  video_encoder->pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
  if (container->oformat->flags & AVFMT_GLOBALHEADER)
    video_encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Open the codec, and set some x264 preferences */
  bail_if(avcodec_open2(video_encoder, codec, NULL), "avcodec_open2");
  if (codec->id == AV_CODEC_ID_H264){
    bail_if(av_opt_set(video_encoder->priv_data, "preset", "slow", 0), "Set x264 preset to slow");
    //bail_if(av_opt_set(video_encoder->priv_data, "crf", "0", 0), "Set x264 quality to lossless");
  }

  /* Start a video stream */
  AVStream *video_stream = avformat_new_stream(container, codec);
  bail_if_null(video_stream, "avformat_new_stream");
  bail_if(avcodec_parameters_from_context(video_stream->codecpar, video_encoder), "avcodec_parameters_from_context");
  video_stream->nb_frames = len;

  /* Open output file file */
  if (!(container->oformat->flags & AVFMT_NOFILE))
    bail_if(avio_open(&container->pb, filename, AVIO_FLAG_WRITE), "avio_open");
  bail_if(avformat_write_header(container, NULL), "avformat_write_header");

  /* Store relevant objects */
  output_container *out = (output_container*) av_mallocz(sizeof(output_container));
  out->container = container;
  out->video_stream = video_stream;
  out->video_encoder = video_encoder;

  //print info and return
  av_dump_format(container, 0, filename, 1);
  return out;
}

static void close_output_file(output_container *output){
  bail_if(av_write_trailer(output->container), "av_write_trailer");
  if (!(output->container->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output->container->pb);
  avcodec_close(output->video_encoder);
  avcodec_free_context(&(output->video_encoder));
  avformat_free_context(output->container);
  av_free(output);
}

static AVFrame * read_single_frame(const char *filename){
  AVFormatContext *ifmt_ctx = NULL;
  bail_if(avformat_open_input(&ifmt_ctx, filename, NULL, NULL), "avformat_open_input");
  bail_if(avformat_find_stream_info(ifmt_ctx, NULL), "avformat_find_stream_info");

  /* Try all input streams */
  for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
    AVStream *stream = ifmt_ctx->streams[i];
    if(stream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
      continue;
    AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    bail_if_null(codec, "avcodec_find_decoder");
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    bail_if(avcodec_parameters_to_context(codec_ctx, stream->codecpar), "avcodec_parameters_to_context");
    codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
    bail_if(avcodec_open2(codec_ctx, codec, NULL), "avcodec_open2");
    int ret;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *picture = av_frame_alloc();
    do {
      ret = av_read_frame(ifmt_ctx, pkt);
      if(ret == AVERROR_EOF){
        bail_if(avcodec_send_packet(codec_ctx, NULL), "flushing avcodec_send_packet");
      } else {
        bail_if(ret, "av_read_frame");
        if(pkt->stream_index != i){
          av_packet_unref(pkt);
          continue; //wrong stream
        }
        bail_if(avcodec_send_packet(codec_ctx, pkt), "avcodec_send_packet");
      }
      av_packet_unref(pkt);
      int ret2 = avcodec_receive_frame(codec_ctx, picture);
      if(ret2 == AVERROR(EAGAIN))
        continue;
      bail_if(ret2, "avcodec_receive_frame");
      av_packet_free(&pkt);
      avcodec_close(codec_ctx);
      avcodec_free_context(&codec_ctx);
      avformat_close_input(&ifmt_ctx);
      avformat_free_context(ifmt_ctx);
      return picture;
    } while(ret == 0);
  }
  Rf_error("No suitable stream or frame found");
}

static video_filter *open_filter(AVFrame * input, enum AVPixelFormat fmt, const char *filter_spec){

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


  /* Endpoints for the filter graph. */
  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();
  outputs->name = av_strdup("in");
  outputs->filter_ctx = buffersrc_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;
  inputs->name = av_strdup("out");
  inputs->filter_ctx = buffersink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  /* Parse and init the custom user filter */
  bail_if(avfilter_graph_parse_ptr(filter_graph, filter_spec,
                                   &inputs, &outputs, NULL), "avfilter_graph_parse_ptr");
  bail_if(avfilter_graph_config(filter_graph, NULL), "avfilter_graph_config");
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  video_filter *out = (video_filter*) av_mallocz(sizeof(video_filter));
  out->input = buffersrc_ctx;
  out->output = buffersink_ctx;
  out->graph = filter_graph;
  return out;
}

static void close_video_filter(video_filter *filter){
  for(int i = 0; i < filter->graph->nb_filters; i++)
    avfilter_free(filter->graph->filters[i]);
  avfilter_graph_free(&filter->graph);
  av_free(filter);
}

SEXP R_encode_video(SEXP in_files, SEXP out_file, SEXP framerate, SEXP filterstr, SEXP enc){
  double duration = VIDEO_TIME_BASE / Rf_asReal(framerate);
  AVCodec *codec = NULL;
  if(Rf_length(enc)) {
    codec = avcodec_find_encoder_by_name(CHAR(STRING_ELT(enc, 0)));
  } else {
    AVOutputFormat *frmt = av_guess_format(NULL, CHAR(STRING_ELT(out_file, 0)), NULL);
    bail_if_null(frmt, "av_guess_format");
    codec = avcodec_find_encoder(frmt->video_codec);
  }
  bail_if_null(codec, "avcodec_find_encoder_by_name");
  enum AVPixelFormat pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;

  /* Start the output video */
  AVFrame * frame = NULL;
  video_filter *filter = NULL;
  output_container *output = NULL;
  AVPacket *pkt = av_packet_alloc();

  /* Loop over input image files files */
  for(int i = 0; i <= Rf_length(in_files); i++){
    if(i < Rf_length(in_files)) {
      frame = read_single_frame(CHAR(STRING_ELT(in_files, i)));
      frame->pts = i * duration;
      if(filter == NULL)
        filter = open_filter(frame, pix_fmt, CHAR(STRING_ELT(filterstr, 0)));
      bail_if(av_buffersrc_add_frame(filter->input, frame), "av_buffersrc_add_frame");
      av_frame_free(&frame);
    } else {
      bail_if_null(filter, "Faild to read any input frames");
      bail_if(av_buffersrc_add_frame(filter->input, NULL), "flushing filter");
    }

    /* Loop over frames returned by filter */
    while(1){
      AVFrame * outframe = av_frame_alloc();
      int ret = av_buffersink_get_frame(filter->output, outframe);
      if(ret == AVERROR(EAGAIN))
        break;
      if(ret == AVERROR_EOF){
        bail_if_null(output, "filter did not return any frames");
        bail_if(avcodec_send_frame(output->video_encoder, NULL), "avcodec_send_frame");
      } else {
        bail_if(ret, "av_buffersink_get_frame");
        outframe->pict_type = AV_PICTURE_TYPE_I;
        if(output == NULL)
          output = open_output_file(CHAR(STRING_ELT(out_file, 0)), outframe->width, outframe->height,
                                    codec, Rf_length(in_files));
        bail_if(avcodec_send_frame(output->video_encoder, outframe), "avcodec_send_frame");
        av_frame_free(&outframe);
      }

      /* re-encode output packet */
      while(1){
        int ret = avcodec_receive_packet(output->video_encoder, pkt);
        if (ret == AVERROR(EAGAIN))
          break;
        if (ret == AVERROR_EOF){
          av_log(NULL, AV_LOG_INFO, " - video stream completed!\n");
          goto done;
        }
        bail_if(ret, "avcodec_receive_packet");
        //pkt->duration = duration; <-- may have changed by the filter!
        pkt->stream_index = output->video_stream->index;
        av_log(NULL, AV_LOG_INFO, "\rAdding frame %d at timestamp %.2fsec (%d%%)",
               i, (double) pkt->pts / VIDEO_TIME_BASE, i * 100 / Rf_length(in_files));
        av_packet_rescale_ts(pkt, output->video_encoder->time_base, output->video_stream->time_base);
        bail_if(av_interleaved_write_frame(output->container, pkt), "av_interleaved_write_frame");
        av_packet_unref(pkt);
      }
    }
  }
  Rf_warning("Did not reach EOF, video may be incomplete");
done:
  close_video_filter(filter);
  close_output_file(output);
  av_packet_free(&pkt);
  return out_file;
}
