#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

typedef struct {
  AVFormatContext *fmt_ctx;
  AVCodecContext *codec_ctx;
  AVStream *video_stream;
} video_stream;

static void log_debug(const char * str){
  av_log(NULL, AV_LOG_DEBUG, "%s", str);
}

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(void * ptr, const char * what){
  if(!ptr)
    bail_if(-1, what);
}

static video_stream *open_output_file(const char *filename, int width, int height, int framerate, const char * enc){
  /* First check if we have the video codec (does not allocate) */
  AVCodec *codec = avcodec_find_encoder_by_name(enc);
  bail_if_null(codec, "avcodec_find_encoder_by_name");

  /* Init container context (infers format from file extension) */
  AVFormatContext *ofmt_ctx = NULL;
  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
  bail_if_null(ofmt_ctx, "avformat_alloc_output_context2");

  /* Init video encoder */
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  bail_if_null(codec_ctx, "avcodec_alloc_context3");
  codec_ctx->height = height;
  codec_ctx->width = width;
  codec_ctx->time_base.num = 1;
  codec_ctx->time_base.den = framerate;
  codec_ctx->framerate = av_inv_q(codec_ctx->time_base);
  codec_ctx->gop_size = 5;
  codec_ctx->max_b_frames = 1;

  /* Try to use codec preferred pixel format, otherwise default to YUV420 */
  codec_ctx->pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
  if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Open the codec, and set some x264 preferences */
  bail_if(avcodec_open2(codec_ctx, codec, NULL), "avcodec_open2");
  if (codec->id == AV_CODEC_ID_H264){
    bail_if(av_opt_set(codec_ctx->priv_data, "preset", "slow", 0), "Set x264 preset to slow");
    bail_if(av_opt_set(codec_ctx->priv_data, "crf", "0", 0), "Set x264 quality to lossless");
  }

  /* Start a video stream */
  AVStream *out_stream = avformat_new_stream(ofmt_ctx, codec);
  bail_if_null(out_stream, "avformat_new_stream");
  bail_if(avcodec_parameters_from_context(out_stream->codecpar, codec_ctx), "avcodec_parameters_from_context");

  /* Open output file file */
  if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    bail_if(avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE), "avio_open");
  bail_if(avformat_write_header(ofmt_ctx, NULL), "avformat_write_header");

  /* Store relevant objects */
  video_stream *out = (video_stream*) av_mallocz(sizeof(video_stream));
  out->fmt_ctx = ofmt_ctx;
  out->video_stream = out_stream;
  out->codec_ctx = codec_ctx;

  //print info and return
  av_dump_format(ofmt_ctx, 0, filename, 1);
  return out;
}

static void close_output_file(video_stream *output){
  bail_if(av_write_trailer(output->fmt_ctx), "av_write_trailer");
  if (!(output->fmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output->fmt_ctx->pb);
  avcodec_close(output->codec_ctx);
  avcodec_free_context(&(output->codec_ctx));
  avformat_free_context(output->fmt_ctx);
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

static AVFrame *filter_single_frame(AVFrame * input, enum AVPixelFormat fmt, int width, int height){

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


  /* Add custom filters */
  AVFilterContext *filter_tail = buffersrc_ctx;
  AVFilterContext *next_filter = NULL;

  /* Adding scale filter */
  if(input->width != width || input->height != height){
    char scale_args[512];
    snprintf(scale_args, sizeof(scale_args), "%d:%d", width, height);
    bail_if(avfilter_graph_create_filter(&next_filter, avfilter_get_by_name("scale"),
                                         "scale", scale_args, NULL, filter_graph), "avfilter_graph_create_filter");
    bail_if(avfilter_link(filter_tail, 0, next_filter, 0), "avfilter_link (scale)");
    filter_tail = next_filter;
  }

  /* Hookup to the output filter */
  bail_if(avfilter_link(filter_tail, 0, buffersink_ctx, 0), "avfilter_link (sink)");
  bail_if(avfilter_graph_config(filter_graph, NULL), "avfilter_graph_config");

  /* Insert the frame into the source filter */
  bail_if(av_buffersrc_add_frame_flags(buffersrc_ctx, input, 0), "av_buffersrc_add_frame_flags");

  /* Extract output frame from the sink filter */
  AVFrame * output = av_frame_alloc();
  bail_if(av_buffersink_get_frame(buffersink_ctx, output), "av_buffersink_get_frame");

  for(int i = 0; i < filter_graph->nb_filters; i++)
    avfilter_free(filter_graph->filters[i]);
  avfilter_graph_free(&filter_graph);
  av_frame_free(&input);

  /* Return an output frame */
  output->pict_type = AV_PICTURE_TYPE_I;
  return output;
}

SEXP R_create_video(SEXP in_files, SEXP out_file, SEXP width, SEXP height, SEXP fps, SEXP enc){
  /* Read first image file to get width/height */
  AVFrame * sample = read_single_frame(CHAR(STRING_ELT(in_files, 0)));
  int w = Rf_length(width) ? Rf_asInteger(width) : sample->width;
  int h = Rf_length(height) ? Rf_asInteger(height) : sample->height;
  av_frame_free(&sample);

  /* Start the output video */
  video_stream *output = open_output_file(CHAR(STRING_ELT(out_file, 0)), w, h,
                                          Rf_asInteger(fps), CHAR(STRING_ELT(enc, 0)));
  AVPacket *pkt = av_packet_alloc();
  output->video_stream->nb_frames = Rf_length(in_files);

  /* Loop over input image files files */
  int pos = 0;
  for(int i = 0; i <= Rf_length(in_files); i++){
    if(i < Rf_length(in_files)){
      AVFrame * frame = read_single_frame(CHAR(STRING_ELT(in_files, i)));
      frame = filter_single_frame(frame, output->codec_ctx->pix_fmt, Rf_asInteger(width), Rf_asInteger(height));
      frame->pts = i;
      bail_if(avcodec_send_frame(output->codec_ctx, frame), "avcodec_send_frame");
      av_frame_free(&frame);
    } else {
      bail_if(avcodec_send_frame(output->codec_ctx, NULL), "flushing avcodec_send_frame");
    }

    /* re-encode output packet */
    while(1){
      int ret = avcodec_receive_packet(output->codec_ctx, pkt);
      if (ret == AVERROR(EAGAIN))
        break;
      if (ret == AVERROR_EOF)
        goto done;
      bail_if(ret, "avcodec_receive_packet");
      pkt->pos = pos++;
      pkt->duration = 1;
      pkt->stream_index = output->video_stream->index;
      av_packet_rescale_ts(pkt, output->codec_ctx->time_base, output->video_stream->time_base);
      bail_if(av_interleaved_write_frame(output->fmt_ctx, pkt), "av_interleaved_write_frame");
      av_packet_unref(pkt);
    }
  }
done:
  close_output_file(output);
  av_packet_free(&pkt);
  return out_file;
}
