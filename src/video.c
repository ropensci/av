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
  AVCodecContext *video_codec_ctx;
  AVStream *video_stream;
} video_stream;

typedef struct {
  AVFilterContext *input;
  AVFilterContext *output;
  AVFilterGraph *filter_graph;
} video_filter;

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(void * ptr, const char * what){
  if(!ptr)
    bail_if(-1, what);
}

static video_stream *open_output_file(const char *filename, int width, int height, int framerate, AVCodec *codec, int len){
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
  out_stream->nb_frames = len;

  /* Open output file file */
  if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    bail_if(avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE), "avio_open");
  bail_if(avformat_write_header(ofmt_ctx, NULL), "avformat_write_header");

  /* Store relevant objects */
  video_stream *out = (video_stream*) av_mallocz(sizeof(video_stream));
  out->fmt_ctx = ofmt_ctx;
  out->video_stream = out_stream;
  out->video_codec_ctx = codec_ctx;

  //print info and return
  av_dump_format(ofmt_ctx, 0, filename, 1);
  return out;
}

static void close_output_file(video_stream *output){
  bail_if(av_write_trailer(output->fmt_ctx), "av_write_trailer");
  if (!(output->fmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output->fmt_ctx->pb);
  avcodec_close(output->video_codec_ctx);
  avcodec_free_context(&(output->video_codec_ctx));
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
  out->filter_graph = filter_graph;
  return out;
}

static void close_video_filter(video_filter *filter){
  for(int i = 0; i < filter->filter_graph->nb_filters; i++)
    avfilter_free(filter->filter_graph->filters[i]);
  avfilter_graph_free(&filter->filter_graph);
  av_free(filter);
}

SEXP R_encode_video(SEXP in_files, SEXP out_file, SEXP framerate, SEXP filterstr, SEXP enc){
  AVCodec *codec = avcodec_find_encoder_by_name(CHAR(STRING_ELT(enc, 0)));
  bail_if_null(codec, "avcodec_find_encoder_by_name");
  enum AVPixelFormat pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;

  /* Start the output video */
  int ret = 0;
  AVFrame * frame = NULL;
  video_filter *filter = NULL;
  video_stream *outfile = NULL;
  AVPacket *pkt = av_packet_alloc();

  /* Loop over input image files files */
  for(int i = 0; i <= Rf_length(in_files); i++){
    if(i < Rf_length(in_files)) {
      frame = read_single_frame(CHAR(STRING_ELT(in_files, i)));
      frame->pts = i;
    } else {
      frame = NULL; //flush
    }
    if(filter == NULL)
      filter = open_filter(frame, pix_fmt, CHAR(STRING_ELT(filterstr, 0)));
    bail_if(av_buffersrc_add_frame(filter->input, frame), "av_buffersrc_add_frame");

    /* Loop over frames returned by filter */
    while(1){
      AVFrame * outframe = av_frame_alloc();
      ret = av_buffersink_get_frame(filter->output, outframe);
      if(ret == AVERROR(EAGAIN))
        break;
      if(ret == AVERROR_EOF){
        bail_if_null(outfile, "filter did not return any frames");
        bail_if(avcodec_send_frame(outfile->video_codec_ctx, NULL), "avcodec_send_frame");
      } else {
        bail_if(ret, "av_buffersink_get_frame");
        outframe->pict_type = AV_PICTURE_TYPE_I;
        if(outfile == NULL)
          outfile = open_output_file(CHAR(STRING_ELT(out_file, 0)), outframe->width, outframe->height,
                                    Rf_asInteger(framerate), codec, Rf_length(in_files));
        bail_if(avcodec_send_frame(outfile->video_codec_ctx, outframe), "avcodec_send_frame");
        av_frame_free(&outframe);
      }

      /* re-encode output packet */
      while(1){
        int ret = avcodec_receive_packet(outfile->video_codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN))
          break;
        if (ret == AVERROR_EOF)
          goto done;
        bail_if(ret, "avcodec_receive_packet");
        pkt->duration = 1;
        pkt->stream_index = outfile->video_stream->index;
        av_packet_rescale_ts(pkt, outfile->video_codec_ctx->time_base, outfile->video_stream->time_base);
        bail_if(av_interleaved_write_frame(outfile->fmt_ctx, pkt), "av_interleaved_write_frame");
        av_packet_unref(pkt);
      }
    }
  }
  Rf_warning("Failed to complete input");
done:
  close_video_filter(filter);
  close_output_file(outfile);
  av_packet_free(&pkt);
  return out_file;
}
