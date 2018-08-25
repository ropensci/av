#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>
#include <stdlib.h>

typedef struct {
  int index;
  AVFormatContext *fmt_ctx;
  AVCodecContext *codec_ctx;
  AVStream *stream;
  AVCodec *codec;
} video_stream;

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(void * ptr, const char * what){
  if(!ptr)
    bail_if(-1, what);
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
    video_stream *out = (video_stream*) malloc(sizeof(video_stream));
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
  video_stream *out = (video_stream*) malloc(sizeof(video_stream));
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
  AVPacket *pkt = av_packet_alloc();
  while(1){
    Rprintf("Trying to read a packet...\n");
    ret = av_read_frame(input->fmt_ctx, pkt);
    if(ret == AVERROR_EOF){
      Rprintf("flusing input...\n");
      bail_if(avcodec_send_packet(input->codec_ctx, NULL), "flushing avcodec_send_packet");
    } else {
      bail_if(ret, "av_read_frame");
      if(pkt->stream_index != input->index){
        av_packet_unref(pkt);
        continue; //wrong stream
      }
      Rprintf("Sending the packet...\n");
      bail_if(avcodec_send_packet(input->codec_ctx, pkt), "avcodec_send_packet");
    }
    av_packet_unref(pkt);
    while(1){
      Rprintf("Receiving frame...\n");
      AVFrame *picture = av_frame_alloc();
      ret = avcodec_receive_frame(input->codec_ctx, picture);
      if (ret == AVERROR(EAGAIN))
        break;
      if(ret == AVERROR_EOF){
        Rprintf("Flushing output...\n");
        bail_if(avcodec_send_frame(output->codec_ctx, NULL), "flushing avcodec_send_frame");
      } else {
        Rprintf("Sending frame to encoder...\n");
        bail_if(ret, "avcodec_receive_frame");
        picture->pts = picture->best_effort_timestamp;
        bail_if(avcodec_send_frame(output->codec_ctx, picture), "avcodec_send_frame");
        av_frame_free(&picture);
      }
      
      /* re-encode output packet */
      while(1){
        Rprintf("Trying to read packet from encoder...\n");
        ret = avcodec_receive_packet(output->codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          break;
        bail_if(ret, "avcodec_receive_packet");
        pkt->stream_index = output->index;
        av_packet_rescale_ts(pkt, input->codec_ctx->time_base, output->codec_ctx->time_base);
        Rprintf("Writing packet to output file...\n");
        bail_if(av_interleaved_write_frame(output->fmt_ctx, pkt), "av_interleaved_write_frame");
        av_packet_unref(pkt);
      }
    }
  }
  av_packet_free(&pkt);
  av_write_trailer(output->fmt_ctx);
  avcodec_free_context(&(input->codec_ctx));
  avcodec_free_context(&(output->codec_ctx));
  av_free(input->stream);
  av_free(output->stream);
  avformat_close_input(&input->fmt_ctx);
  if (!(output->fmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output->fmt_ctx->pb);
  avformat_free_context(output->fmt_ctx);  
  return R_NilValue;
}
