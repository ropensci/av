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
  int index;
  AVFormatContext *fmt_ctx;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  AVCodec *codec;
} video_stream;

typedef struct {
  AVFilterContext *source;
  AVFilterContext *sink;
  AVFilterGraph *graph;
} video_filter;

static void print_log(const char * str){
  av_log(NULL, AV_LOG_INFO, "%s", str);
}

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(void * ptr, const char * what){
  if(!ptr)
    bail_if(-1, what);
}

static video_filter *create_video_filter(AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char * filter_spec){
  video_filter * fctx = (video_filter *) av_mallocz(sizeof(video_filter));
  AVFilterGraph *filter_graph = avfilter_graph_alloc();

  /* define the filter string */
  char args[512];
  snprintf(args, sizeof(args),
           "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
           dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
           dec_ctx->time_base.num, dec_ctx->time_base.den,
           dec_ctx->sample_aspect_ratio.num,
           dec_ctx->sample_aspect_ratio.den);
  
  AVFilterContext *buffersrc_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("buffer"), "in", 
                                       args, NULL, filter_graph), "avfilter_graph_create_filter (input)");
  
  AVFilterContext *buffersink_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("buffersink"), "out",
                               NULL, NULL, filter_graph), "avfilter_graph_create_filter (output)");
  
  bail_if(av_opt_set_bin(buffersink_ctx, "pix_fmts",
                       (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
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
  
  /* Not sure what this does */
  bail_if(avfilter_graph_parse_ptr(filter_graph, filter_spec,
                           &inputs, &outputs, NULL), "avfilter_graph_parse_ptr");
  bail_if(avfilter_graph_config(filter_graph, NULL), "avfilter_graph_config");
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);
  
  fctx->source = buffersrc_ctx;
  fctx->sink = buffersink_ctx;
  fctx->graph = filter_graph;
  return fctx;
}

static video_stream *open_input_file(const char *filename){
  AVFormatContext *ifmt_ctx = NULL;
  bail_if(avformat_open_input(&ifmt_ctx, filename, NULL, NULL), "avformat_open_input");
  bail_if(avformat_find_stream_info(ifmt_ctx, NULL), "avformat_find_stream_info");

  /* Try  all input streams */
  for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
    AVStream *stream = ifmt_ctx->streams[i];
    AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    bail_if_null(codec, "avcodec_find_decoder");
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (codec_ctx->codec_type != AVMEDIA_TYPE_VIDEO)
      continue;
    bail_if(avcodec_parameters_to_context(codec_ctx, stream->codecpar), "avcodec_parameters_to_context");
    codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
    bail_if(avcodec_open2(codec_ctx, codec, NULL), "avcodec_open2");

    //create struct with all objects
    video_stream *out = (video_stream*) av_mallocz(sizeof(video_stream));
    out->fmt_ctx = ifmt_ctx;
    out->stream = stream;
    out->codec_ctx = codec_ctx;
    out->codec = codec;
    out->index = i;

    //print info and return
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return out;
  }
  Rf_error("No suitable stream found");
}

static video_stream * open_output_file(const char *filename, int width, int height){
  AVFormatContext *ofmt_ctx = NULL;
  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
  bail_if_null(ofmt_ctx, "avformat_alloc_output_context2");
  AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
  bail_if_null(out_stream, "avformat_new_stream");
  AVCodec *codec = avcodec_find_encoder_by_name("libx264");
  bail_if_null(codec, "avcodec_find_encoder_by_name");
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  bail_if_null(codec_ctx, "avcodec_alloc_context3");

  /* TO DO: parameterize these these settings */
  codec_ctx->height = height;
  codec_ctx->width = width;
  codec_ctx->sample_aspect_ratio = (AVRational){1, 1};
  codec_ctx->time_base = (AVRational){1, 1};
  codec_ctx->framerate = (AVRational){1, 1};
  codec_ctx->pix_fmt = codec->pix_fmts ? codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
  if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  bail_if(avcodec_open2(codec_ctx, codec, NULL), "avcodec_open2");

  /* Start out stream */
  bail_if(avcodec_parameters_from_context(out_stream->codecpar, codec_ctx), "avcodec_parameters_from_context");
  out_stream->time_base = codec_ctx->time_base;

  if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    bail_if(avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE), "avio_open");

  bail_if(avformat_write_header(ofmt_ctx, NULL), "avformat_write_header");

  //create struct with all objects
  video_stream *out = (video_stream*) av_mallocz(sizeof(video_stream));
  out->fmt_ctx = ofmt_ctx;
  out->stream = out_stream;
  out->codec_ctx = codec_ctx;
  out->codec = codec;
  out->index = 0;

  //print info and return
  av_dump_format(ofmt_ctx, 0, filename, 1);
  return out;
}

SEXP R_convert(SEXP in_file, SEXP out_file){
  int ret;
  video_stream *input = open_input_file(CHAR(STRING_ELT(in_file, 0)));
  video_stream *output = open_output_file(
    CHAR(STRING_ELT(out_file, 0)), input->codec_ctx->width, input->codec_ctx->height);
  video_filter *filter = create_video_filter(input->codec_ctx, output->codec_ctx, "null");
  AVPacket *pkt = av_packet_alloc();
  while(1){
    ret = av_read_frame(input->fmt_ctx, pkt);
    if(ret == AVERROR_EOF){
      print_log("Input ended. Flushing decoder...\n");
      bail_if(avcodec_send_packet(input->codec_ctx, NULL), "flushing avcodec_send_packet");
    } else {
      bail_if(ret, "av_read_frame");
      if(pkt->stream_index != input->index){
        av_packet_unref(pkt);
        continue; //wrong stream
      }
      print_log("Got packet from input. Sending to decoder...\n");
      bail_if(avcodec_send_packet(input->codec_ctx, pkt), "avcodec_send_packet");
    }
    av_packet_unref(pkt);
    while(1){
      AVFrame *picture = av_frame_alloc();
      ret = avcodec_receive_frame(input->codec_ctx, picture);
      if (ret == AVERROR(EAGAIN))
        break;
      if(ret == AVERROR_EOF){
        print_log("Decoder ended. Flushing encoder...\n");
        bail_if(avcodec_send_frame(output->codec_ctx, NULL), "flushing avcodec_send_frame");
      } else {
        bail_if(ret, "avcodec_receive_frame");
        picture->pts = picture->best_effort_timestamp;
        
        /* apply the filtering */
        print_log("Got frame from decoder. Applying filters...\n");
        bail_if(av_buffersrc_add_frame_flags(filter->source, picture, 0), "av_buffersrc_add_frame_flags");
        AVFrame * filt_frame = av_frame_alloc();
        bail_if(av_buffersink_get_frame(filter->sink, filt_frame), "av_buffersink_get_frame");
        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        av_frame_free(&picture);
        
        /* Feed it to the output encoder */
        print_log("Filtering OK. Sending frame to encoder...\n");
        bail_if(avcodec_send_frame(output->codec_ctx, filt_frame), "avcodec_send_frame");
        av_frame_free(&filt_frame);
      }
      
      /* re-encode output packet */
      while(1){
        ret = avcodec_receive_packet(output->codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN))
          break;
        if (ret == AVERROR_EOF)
          goto done;
        bail_if(ret, "avcodec_receive_packet");
        pkt->stream_index = output->index;
        av_packet_rescale_ts(pkt, input->codec_ctx->time_base, output->codec_ctx->time_base);
        print_log("Got package from encoder. Writing to output...\n");
        bail_if(av_interleaved_write_frame(output->fmt_ctx, pkt), "av_interleaved_write_frame");
        av_packet_unref(pkt);
      }
    }
  }

done:
  av_packet_free(&pkt);
  av_write_trailer(output->fmt_ctx);
  avcodec_free_context(&(input->codec_ctx));
  avcodec_free_context(&(output->codec_ctx));
  avfilter_graph_free(&filter->graph);
  avformat_close_input(&input->fmt_ctx);
  if (!(output->fmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output->fmt_ctx->pb);
  avformat_free_context(output->fmt_ctx);  
  av_free(filter);
  av_free(input);
  av_free(output);
  return R_NilValue;
}
