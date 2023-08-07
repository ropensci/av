#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>

#define R_NO_REMAP
#define STRICT_R_HEADERS
#define PTS_EVERYTHING 1e18
#define VIDEO_TIME_BASE 1000
#include <Rinternals.h>
#include "avcompat.h"

int total_open_handles = 0;

typedef struct {
  int completed;
  AVFormatContext *demuxer;
  AVCodecContext *decoder;
  AVStream *stream;
} input_container;

typedef struct {
  AVFilterContext *input;
  AVFilterContext *output;
  AVFilterGraph *graph;
} filter_container;

typedef struct {
  const AVCodec *codec;
  AVFormatContext *muxer;
  input_container *audio_input;
  input_container *video_input;
  AVStream *video_stream;
  AVStream *audio_stream;
  filter_container *video_filter;
  filter_container *audio_filter;
  AVCodecContext *video_encoder;
  AVCodecContext *audio_encoder;
  const char * filter_string;
  const char * output_file;
  const char * format_name;
  double duration;
  int64_t end_pts;
  int64_t max_pts;
  int64_t count;
  int progress_pct;
  int channels;
  int sample_rate;
  SEXP in_files;
} output_container;

static void warn_if(int ret, const char * what){
  if(ret < 0)
    Rf_warningcall_immediate(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(const void * ptr, const char * what){
  if(!ptr)
    bail_if(-1, what);
}

static input_container * new_input_container(AVFormatContext *demuxer, AVCodecContext *decoder, AVStream *stream){
  input_container *out = (input_container*) av_mallocz(sizeof(input_container));
  out->demuxer = demuxer;
  out->stream = stream;
  out->decoder = decoder;
  return out;
}

static void close_input(input_container **x){
  input_container *input = *x;
  if(input == NULL)
    return;
  avcodec_close(input->decoder);
  avcodec_free_context(&(input->decoder));
  avformat_close_input(&input->demuxer);
  avformat_free_context(input->demuxer);
  av_free(input);
  *x = NULL;
}

static filter_container * new_filter_container(AVFilterContext *input, AVFilterContext *output, AVFilterGraph *graph){
  filter_container *out = (filter_container*) av_mallocz(sizeof(filter_container));
  out->input = input;
  out->output = output;
  out->graph = graph;
  return out;
}

static void close_filter_container(filter_container *filter){
  for(int i = 0; i < filter->graph->nb_filters; i++)
    avfilter_free(filter->graph->filters[i]);
  avfilter_graph_free(&filter->graph);
  av_free(filter);
}

static void close_output_file(void *ptr, Rboolean jump){
  total_open_handles--;
  output_container *output = ptr;
  if(output->audio_input != NULL){
    close_input(&output->audio_input);
  }
  if(output->video_input != NULL){
    close_input(&output->video_input);
  }
  if(output->video_encoder != NULL){
    close_filter_container(output->video_filter);
    avcodec_close(output->video_encoder);
    avcodec_free_context(&(output->video_encoder));
  }
  if(output->audio_encoder != NULL){
    close_filter_container(output->audio_filter);
    avcodec_close(output->audio_encoder);
    avcodec_free_context(&(output->audio_encoder));
  }
  if(output->muxer != NULL){
    if(output->muxer->pb){
      warn_if(av_write_trailer(output->muxer), "av_write_trailer");
      if (!(output->muxer->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output->muxer->pb);
    }
    avformat_close_input(&output->muxer);
    avformat_free_context(output->muxer);
  }
  av_free(output);
}

static int find_stream_type(AVFormatContext *demuxer, enum AVMediaType type){
  for (int si = 0; si < demuxer->nb_streams; si++) {
    AVStream *stream = demuxer->streams[si];
    if(stream->codecpar->codec_type == type)
      return si;
  }
  return -1;
}

static int find_stream_video(AVFormatContext *demuxer, const char *file){
  int out = find_stream_type(demuxer, AVMEDIA_TYPE_VIDEO);
  if(out < 0){
    avformat_close_input(&demuxer);
    avformat_free_context(demuxer);
    Rf_error("Input %s does not contain suitable video stream", file);
  }
  return out;
}

static int find_stream_audio(AVFormatContext *demuxer, const char *file){
  int out = find_stream_type(demuxer, AVMEDIA_TYPE_AUDIO);
  if(out < 0){
    avformat_close_input(&demuxer);
    avformat_free_context(demuxer);
    Rf_error("Input %s does not contain suitable audio stream", file);
  }
  return out;
}

static input_container *open_audio_input(const char *filename){
  AVFormatContext *demuxer = NULL;
  bail_if(avformat_open_input(&demuxer, filename, NULL, NULL), "avformat_open_input");
  bail_if(avformat_find_stream_info(demuxer, NULL), "avformat_find_stream_info");

  /* Try all input streams */
  int si = find_stream_audio(demuxer, filename);
  AVStream *stream = demuxer->streams[si];
  const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  bail_if_null(codec, "avcodec_find_decoder");
  AVCodecContext *decoder = avcodec_alloc_context3(codec);
  bail_if(avcodec_parameters_to_context(decoder, stream->codecpar), "avcodec_parameters_to_context");
  bail_if(avcodec_open2(decoder, codec, NULL), "avcodec_open2 (audio)");
#ifdef NEW_CHANNEL_API
  if (decoder->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
    av_channel_layout_default(&decoder->ch_layout, decoder->ch_layout.nb_channels);
#else
  if(!decoder->channel_layout)
    decoder->channel_layout = av_get_default_channel_layout(decoder->channels);
#endif
  return new_input_container(demuxer, decoder, demuxer->streams[si]);
}

static filter_container *open_audio_filter(AVCodecContext *decoder, AVCodecContext *encoder, const char *filter_spec){

  /* Create a new filter graph */
  AVFilterGraph *filter_graph = avfilter_graph_alloc();
  char input_args[512];

#ifdef NEW_CHANNEL_API
  char buf[64] = {0};
  av_channel_layout_describe(&decoder->ch_layout, buf, sizeof(buf));
  snprintf(input_args, sizeof(input_args),
           "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
           decoder->time_base.num, decoder->time_base.den, decoder->sample_rate,
           av_get_sample_fmt_name(decoder->sample_fmt),
           buf);
#else
  snprintf(input_args, sizeof(input_args),
           "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
           decoder->time_base.num, decoder->time_base.den, decoder->sample_rate,
           av_get_sample_fmt_name(decoder->sample_fmt),
           decoder->channel_layout);
#endif
  AVFilterContext *buffersrc_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("abuffer"), "audiosrc",
                                       input_args, NULL, filter_graph), "avfilter_graph_create_filter (audio/src)");

  /* Initiate sink filter */
  AVFilterContext *buffersink_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("abuffersink"), "audiosink",
                                       NULL, NULL, filter_graph), "avfilter_graph_create_filter (audio/sink)");

  /* Set output properties (copied from ffmpeg examples/transcoding.c) */
  bail_if(av_opt_set_bin(buffersink_ctx, "sample_fmts",
                         (uint8_t*)&encoder->sample_fmt, sizeof(encoder->sample_fmt),
                         AV_OPT_SEARCH_CHILDREN), "av_opt_set_bin (sample_fmts)");
#ifdef NEW_CHANNEL_API
  av_channel_layout_describe(&encoder->ch_layout, buf, sizeof(buf));
  bail_if(av_opt_set(buffersink_ctx, "ch_layouts",
                   buf, AV_OPT_SEARCH_CHILDREN), "av_opt_set (ch_layouts)");
#else
  bail_if(av_opt_set_bin(buffersink_ctx, "channel_layouts",
                         (uint8_t*)&encoder->channel_layout, sizeof(encoder->channel_layout),
                         AV_OPT_SEARCH_CHILDREN), "av_opt_set_bin (channel_layouts)");
#endif
  bail_if(av_opt_set_bin(buffersink_ctx, "sample_rates",
                         (uint8_t*)&encoder->sample_rate, sizeof(encoder->sample_rate),
                         AV_OPT_SEARCH_CHILDREN), "av_opt_set_bin (sample_rates)");

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
  av_buffersink_set_frame_size(buffersink_ctx, encoder->frame_size);
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);
  return new_filter_container(buffersrc_ctx, buffersink_ctx, filter_graph);
}

static filter_container *open_video_filter(AVFrame * input, enum AVPixelFormat fmt, const char *filter_spec){

  /* Create a new filter graph */
  AVFilterGraph *filter_graph = avfilter_graph_alloc();

  /* Initiate source filter */
  char input_args[512];
  snprintf(input_args, sizeof(input_args),
           "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
           input->width, input->height, input->format, 1, VIDEO_TIME_BASE,
           input->sample_aspect_ratio.num, input->sample_aspect_ratio.den);
  AVFilterContext *buffersrc_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("buffer"), "videosrc",
                                       input_args, NULL, filter_graph), "avfilter_graph_create_filter (video/src)");

  /* Initiate sink filter */
  AVFilterContext *buffersink_ctx = NULL;
  bail_if(avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("buffersink"), "videosink",
                                       NULL, NULL, filter_graph), "avfilter_graph_create_filter (video/sink)");

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
  return new_filter_container(buffersrc_ctx, buffersink_ctx, filter_graph);
}

static void add_video_output(output_container *output, int width, int height){
  AVCodecContext *video_encoder = avcodec_alloc_context3(output->codec);
  bail_if_null(video_encoder, "avcodec_alloc_context3");
  video_encoder->height = height;
  video_encoder->width = width;
  video_encoder->time_base.num = 1;
  video_encoder->time_base.den = VIDEO_TIME_BASE;
  video_encoder->framerate = av_inv_q(video_encoder->time_base);

  /* 2020: disabled because is increase the filesize a lot */
  //video_encoder->gop_size = 25; //one keyframe every 25 frames

  /* Try to use codec preferred pixel format, otherwise default to YUV420 */
  video_encoder->pix_fmt = output->codec->pix_fmts ? output->codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
  if (output->muxer->oformat->flags & AVFMT_GLOBALHEADER)
    video_encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Open the codec, and set some x264 preferences */
  bail_if(avcodec_open2(video_encoder, output->codec, NULL), "avcodec_open2");
  if (output->codec->id == AV_CODEC_ID_H264){
    bail_if(av_opt_set(video_encoder->priv_data, "preset", "slow", 0), "Set x264 preset to slow");
    //bail_if(av_opt_set(video_encoder->priv_data, "crf", "0", 0), "Set x264 quality to lossless");
  }

  /* Start a video stream */
  AVStream *video_stream = avformat_new_stream(output->muxer, output->codec);
  bail_if_null(video_stream, "avformat_new_stream");
  bail_if(avcodec_parameters_from_context(video_stream->codecpar, video_encoder), "avcodec_parameters_from_context");

  /* Store relevant objects */
  output->video_stream = video_stream;
  output->video_encoder = video_encoder;
}

static void add_audio_output(output_container *container){
  AVCodecContext *audio_decoder = container->audio_input->decoder;
  const AVCodec *output_codec = avcodec_find_encoder(container->muxer->oformat->audio_codec);
  bail_if_null(output_codec, "Failed to find default audio codec");
  AVCodecContext *audio_encoder = avcodec_alloc_context3(output_codec);
  bail_if_null(audio_encoder, "avcodec_alloc_context3 (audio)");
#ifdef NEW_CHANNEL_API
  av_channel_layout_default(&audio_encoder->ch_layout, container->channels ? container->channels : audio_decoder->ch_layout.nb_channels);
#else
  audio_encoder->channels = container->channels ? container->channels : audio_decoder->channels;
  audio_encoder->channel_layout = av_get_default_channel_layout(audio_encoder->channels);
#endif
  audio_encoder->sample_rate = container->sample_rate ? container->sample_rate : audio_decoder->sample_rate;
  audio_encoder->sample_fmt = output_codec->sample_fmts[0];
  audio_encoder->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  /* Add the audio_stream to the muxer */
  AVStream *audio_stream = avformat_new_stream(container->muxer, output_codec);
  audio_stream->time_base.den = audio_decoder->sample_rate;
  audio_stream->time_base.num = 1;
  bail_if(avcodec_open2(audio_encoder, output_codec, NULL), "avcodec_open2 (audio)");
  bail_if(avcodec_parameters_from_context(audio_stream->codecpar, audio_encoder), "avcodec_parameters_from_context (audio)");
  container->audio_filter = open_audio_filter(audio_decoder, audio_encoder, "anull");
  container->audio_encoder = audio_encoder;
  container->audio_stream = audio_stream;
}

static void open_output_file(int width, int height, output_container *output){
  /* Init container context (infers format from file extension) */
  AVFormatContext *muxer = NULL;
  avformat_alloc_output_context2(&muxer, NULL, output->format_name, output->output_file);
  bail_if_null(muxer, "avformat_alloc_output_context2");
  output->muxer = muxer;

  /* Init video encoder */
  if(output->in_files != NULL)
    add_video_output(output, width, height);

  /* Add audio stream if needed */
  if(output->audio_input != NULL)
    add_audio_output(output);

  /* Open output file file */
  if (!(muxer->oformat->flags & AVFMT_NOFILE))
    bail_if(avio_open(&muxer->pb, output->output_file, AVIO_FLAG_WRITE), "avio_open");
  bail_if(avformat_write_header(muxer, NULL), "avformat_write_header");

  //print info and return
  av_dump_format(muxer, 0, output->output_file, 1);
}

static void sync_audio_stream(output_container * output, int64_t pts){
  int force_flush = pts == -1;
  int force_everything = pts == PTS_EVERYTHING;
  input_container * input = output->audio_input;
  AVStream *audio_stream = output->audio_stream;
  if(input == NULL || input->completed)
    return;
  static AVPacket *pkt = NULL;
  static AVFrame *frame = NULL;
  if(pkt == NULL){
    pkt = av_packet_alloc();
    frame = av_frame_alloc();
  }
  while(force_everything || force_flush ||
        av_compare_ts(output->end_pts, audio_stream->time_base,
                                     pts, output->video_stream->time_base) < 0) {
    int ret = avcodec_receive_packet(output->audio_encoder, pkt);
    if (ret == AVERROR(EAGAIN)){
      while(1){
        ret = av_buffersink_get_frame(output->audio_filter->output, frame);
        if(ret == AVERROR(EAGAIN)){
          while(1){
            ret = avcodec_receive_frame(input->decoder, frame);
            if(ret == AVERROR(EAGAIN)){
              int ret = av_read_frame(input->demuxer, pkt);
              if(ret == AVERROR_EOF || force_flush){
                bail_if(avcodec_send_packet(input->decoder, NULL), "avcodec_send_packet (flush)");
              } else {
                bail_if(ret, "av_read_frame");
                if(pkt->stream_index == input->stream->index){
                  av_packet_rescale_ts(pkt, input->stream->time_base, input->decoder->time_base);
                  bail_if(avcodec_send_packet(input->decoder, pkt), "avcodec_send_packet (audio)");
                  av_packet_unref(pkt);
                }
              }
            } else if(ret == AVERROR_EOF || force_flush){
              bail_if(av_buffersrc_add_frame(output->audio_filter->input, NULL), "flushing filter");
              break;
            } else {
              bail_if(ret, "avcodec_receive_frame");
              bail_if(av_buffersrc_add_frame(output->audio_filter->input, frame), "av_buffersrc_add_frame");
              av_frame_unref(frame);
              break;
            }
          }
        } else if(ret == AVERROR_EOF){
          bail_if(avcodec_send_frame(output->audio_encoder, NULL), "avcodec_send_frame (audio flush)");
          break;
        } else {
          bail_if(ret, "avcodec_receive_frame (audio)");
          bail_if(avcodec_send_frame(output->audio_encoder, frame), "avcodec_send_frame (audio)");
          av_frame_unref(frame);
          break;
        }
      }
    } else if (ret == AVERROR_EOF){
      av_log(NULL, AV_LOG_INFO, " - audio stream completed!\n");
      input->completed = 1;
      break;
    } else {
      pkt->stream_index = audio_stream->index;
      output->end_pts = pkt->pts + pkt->duration; //cf: av_stream_get_end_pts
      av_packet_rescale_ts(pkt, output->audio_encoder->time_base, audio_stream->time_base);
      bail_if(av_interleaved_write_frame(output->muxer, pkt), "av_interleaved_write_frame");
      int64_t elapsed_pts = av_rescale_q(output->end_pts, audio_stream->time_base, AV_TIME_BASE_Q);
      if(force_everything){
        av_log(NULL, AV_LOG_INFO, "\rAdding audio frame %d at timestamp %.2fsec",
               (int) audio_stream->nb_frames + 1, (double) elapsed_pts / AV_TIME_BASE);
      }
      if(output->max_pts > 0 && output->max_pts < elapsed_pts){
        force_flush = 1;
      };
      R_CheckUserInterrupt();
      av_packet_unref(pkt);
    }
  }
  av_packet_unref(pkt);
  av_frame_unref(frame);
}

static int recode_output_packet(output_container *output){
  static AVPacket *pkt = NULL;
  if(pkt == NULL)
    pkt = av_packet_alloc();
  while(1){
    int ret = avcodec_receive_packet(output->video_encoder, pkt);
    if (ret == AVERROR(EAGAIN))
      return 0;
    if (ret == AVERROR_EOF){
      av_log(NULL, AV_LOG_INFO, " - video stream completed!\n");
      return 1;
    }
    bail_if(ret, "avcodec_receive_packet");
    pkt->stream_index = output->video_stream->index;
    av_log(NULL, AV_LOG_INFO, "\rAdding frame %d at timestamp %.2fsec (%d%%)",
           (int) output->video_stream->nb_frames + 1, (double) pkt->pts / VIDEO_TIME_BASE, output->progress_pct);
    av_packet_rescale_ts(pkt, output->video_encoder->time_base, output->video_stream->time_base);
    sync_audio_stream(output, pkt->pts);
    bail_if(av_interleaved_write_frame(output->muxer, pkt), "av_interleaved_write_frame");
    av_packet_unref(pkt);
    R_CheckUserInterrupt();
  }
}

/* Loop over frames returned by filter */
static int encode_output_frames(output_container *output){
  static AVFrame * frame = NULL;
  if(frame == NULL)
    frame = av_frame_alloc();
  while(1){
    int ret = av_buffersink_get_frame(output->video_filter->output, frame);
    if(ret == AVERROR(EAGAIN))
      return 0;
    if(ret == AVERROR_EOF){
      bail_if_null(output, "filter did not return any frames");
      bail_if(avcodec_send_frame(output->video_encoder, NULL), "avcodec_send_frame (flush video)");
    } else {
      bail_if(ret, "av_buffersink_get_frame");
      if(output->muxer == NULL)
        open_output_file(frame->width, frame->height, output);
      frame->quality = output->video_encoder->global_quality;
      bail_if(avcodec_send_frame(output->video_encoder, frame), "avcodec_send_frame");
      av_frame_unref(frame);
    }

    /* re-encode output packet */
    if(recode_output_packet(output))
      return 1;
  }
}

/* We keep a reference to the previous frame, because we want to add
 * a copy of that frame when we finalize the video.
 */
static int feed_to_filter(AVFrame * image, output_container *output){
  enum AVPixelFormat pix_fmt = output->codec->pix_fmts ? output->codec->pix_fmts[0] : AV_PIX_FMT_YUV420P;
  static AVFrame *previous = NULL;
  if(previous == NULL)
    previous = av_frame_alloc();
  if(output->video_filter == NULL){
    if(image == NULL){
      Rf_error("Failed to read any input images");
    } else {
      output->video_filter = open_video_filter(image, pix_fmt, output->filter_string);
    }
  }
  if(image != NULL){
    /* Release the previous image and update with current one. This should be cheap. */
    av_frame_unref(previous);
    av_frame_ref(previous, image);
  } else {
    /* Add a copy of the final frame before closing the filter */
    previous->pts = (output->count++) * output->duration;
    bail_if(av_buffersrc_add_frame(output->video_filter->input, previous), "av_buffersrc_add_frame");
    av_frame_free(&previous);
  }
  bail_if(av_buffersrc_add_frame(output->video_filter->input, image), "av_buffersrc_add_frame");
  return encode_output_frames(output);
}

static void read_from_input(const char *filename, output_container *output){
  AVFormatContext *demuxer = NULL;
  bail_if(avformat_open_input(&demuxer, filename, NULL, NULL), "avformat_open_input");
  bail_if(avformat_find_stream_info(demuxer, NULL), "avformat_find_stream_info");
  int si = find_stream_video(demuxer, filename);
  AVStream *stream = demuxer->streams[si];
  const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  bail_if_null(codec, "avcodec_find_decoder");
  AVCodecContext *decoder = avcodec_alloc_context3(codec);

  /* This cleans input on.exit */
  output->video_input = new_input_container(demuxer, decoder, stream);
  bail_if(avcodec_parameters_to_context(decoder, stream->codecpar), "avcodec_parameters_to_context");
  decoder->framerate = av_guess_frame_rate(demuxer, stream, NULL);
  bail_if(avcodec_open2(decoder, codec, NULL), "avcodec_open2");

  /* Allocate data storage */
  static AVPacket *pkt = NULL;
  static AVFrame *picture = NULL;
  if(pkt == NULL){
    pkt = av_packet_alloc();
    picture = av_frame_alloc();
  }
  int ret;
  do {
    ret = av_read_frame(demuxer, pkt);
    if(ret == AVERROR_EOF){
      bail_if(avcodec_send_packet(decoder, NULL), "flushing avcodec_send_packet");
    } else {
      bail_if(ret, "av_read_frame");
      if(pkt->stream_index != si){
        av_packet_unref(pkt);
        continue; //wrong stream
      }
      bail_if(avcodec_send_packet(decoder, pkt), "avcodec_send_packet");
    }
    av_packet_unref(pkt);
    int ret2 = avcodec_receive_frame(decoder, picture);
    if(ret2 == AVERROR(EAGAIN))
      continue;
    if(ret2 == AVERROR_EOF)
      break;
    bail_if(ret2, "avcodec_receive_frame");
    picture->pts = (output->count++) * output->duration;
    //prevent keyframe at each image
    //todo: find a way to do this for all length 1 input formats
    if(decoder->codec->id == AV_CODEC_ID_PNG || decoder->codec->id == AV_CODEC_ID_MJPEG)
      picture->pict_type = AV_PICTURE_TYPE_NONE;
    feed_to_filter(picture, output);
  } while(ret != AVERROR_EOF);
  close_input(&output->video_input);
}

/* Loop over input image files files */
static SEXP encode_input_files(void *ptr){
  total_open_handles++;
  output_container *output = ptr;
  int len = Rf_length(output->in_files);
  for(int fi = 0; fi < len; fi++){
    output->progress_pct = fi * 100 / len;
    read_from_input(CHAR(STRING_ELT(output->in_files, fi)), output);
  }
  if(!feed_to_filter(NULL, output))
    Rf_warning("Did not reach EOF, video may be incomplete");

  /* Flush audio stream */
  sync_audio_stream(output, -1);
  return R_NilValue;
}

static const AVCodec *get_default_codec(const char *filename){
  const AVOutputFormat *frmt = av_guess_format(NULL, filename, NULL);
  bail_if_null(frmt, "av_guess_format");
  return avcodec_find_encoder(frmt->video_codec);
}

SEXP R_encode_video(SEXP in_files, SEXP out_file, SEXP framerate, SEXP vfilter,
                    SEXP enc, SEXP audio){
  const AVCodec *codec = Rf_length(enc) ?
    avcodec_find_encoder_by_name(CHAR(STRING_ELT(enc, 0))) :
    get_default_codec(CHAR(STRING_ELT(out_file, 0)));
  bail_if_null(codec, "avcodec_find_encoder_by_name");

  /* Start the output video */
  output_container *output = av_mallocz(sizeof(output_container));
  output->audio_input = Rf_length(audio) ? open_audio_input(CHAR(STRING_ELT(audio, 0))) : NULL;
  output->output_file = CHAR(STRING_ELT(out_file, 0));
  output->duration = VIDEO_TIME_BASE / Rf_asReal(framerate);
  output->filter_string = CHAR(STRING_ELT(vfilter, 0));
  output->codec = codec;
  output->in_files = in_files;
  R_UnwindProtect(encode_input_files, output, close_output_file, output, NULL);
  return out_file;
}

/* Loop over input image files files */
static SEXP encode_audio_input(void *ptr){
  total_open_handles++;
  output_container *output = ptr;
  open_output_file(0, 0, output);
  sync_audio_stream(output, PTS_EVERYTHING);
  return R_NilValue;
}

SEXP R_convert_audio(SEXP audio, SEXP out_file, SEXP out_format, SEXP out_channels,
                     SEXP sample_rate, SEXP start_pos, SEXP max_len){
  output_container *output = av_mallocz(sizeof(output_container));
  if(Rf_length(out_channels))
    output->channels = Rf_asInteger(out_channels);
  if(Rf_length(sample_rate))
    output->sample_rate = Rf_asInteger(sample_rate);
  if(Rf_length(out_format))
    output->format_name = CHAR(STRING_ELT(out_format, 0));
  output->audio_input = open_audio_input(CHAR(STRING_ELT(audio, 0)));
  double start_pts = Rf_length(start_pos) ? Rf_asReal(start_pos) : 0;
  if(start_pts > 0)
    av_seek_frame(output->audio_input->demuxer, -1, start_pts * AV_TIME_BASE, AVSEEK_FLAG_ANY);
  if(Rf_length(max_len))
    output->max_pts = (Rf_asReal(max_len) + start_pts) * AV_TIME_BASE;
  output->output_file = CHAR(STRING_ELT(out_file, 0));
  R_UnwindProtect(encode_audio_input, output, close_output_file, output, NULL);
  return out_file;
}

SEXP R_get_open_handles(void){
  return Rf_ScalarInteger(total_open_handles);
}
