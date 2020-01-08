#include <libavformat/avformat.h>
#include <libavcodec/avfft.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include "window_func.h"

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

enum AmplitudeScale { AS_LINEAR, AS_SQRT, AS_CBRT, AS_LOG, NB_ASCALES };

extern int total_open_handles;

typedef struct {
  int completed;
  AVFormatContext *demuxer;
  AVCodecContext *decoder;
  AVStream *stream;
} input_container;

typedef struct {
  SwrContext *swr;
  FFTContext *fft;
  FFTComplex **fft_data;
  AVAudioFifo *fifo;
  input_container *audio_input;
  int channels;
  int winsize;
  int sample_rate;
  int keep_channels;
  float overlap;
  float *window_func_lut;
  float **src_data;
  double **dst_data;
} spectrum_container;

static void bail_if(int ret, const char * what){
  if(ret < 0)
    Rf_errorcall(R_NilValue, "FFMPEG error in '%s': %s", what, av_err2str(ret));
}

static void bail_if_null(void * ptr, const char * what){
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

static void free_ptr_array(void **ptr, int channels){
  if(ptr != NULL){
    for(int i = 0; i < channels; i++){
      if(ptr[i] != NULL)
        av_free(ptr[i]);
    }
    av_free(ptr);
  }
}

static void close_spectrum_container(void *ptr, Rboolean jump){
  total_open_handles--;
  spectrum_container *s = ptr;
  int channels = s->channels;
  if(s->audio_input != NULL)
    close_input(&s->audio_input);
  if(s->fft)
    av_fft_end(s->fft);
  if(s->fifo)
    av_audio_fifo_free(s->fifo);
  if(s->swr)
    swr_free(&s->swr);
  if(s->window_func_lut)
    av_free(s->window_func_lut);
  free_ptr_array((void**) s->fft_data, channels);
  free_ptr_array((void**) s->src_data, channels);
  free_ptr_array((void**) s->dst_data, channels);
}

static int find_stream_type(AVFormatContext *demuxer, enum AVMediaType type){
  for (int si = 0; si < demuxer->nb_streams; si++) {
    AVStream *stream = demuxer->streams[si];
    if(stream->codecpar->codec_type == type)
      return si;
  }
  return -1;
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
  AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  bail_if_null(codec, "avcodec_find_decoder");
  AVCodecContext *decoder = avcodec_alloc_context3(codec);
  bail_if(avcodec_parameters_to_context(decoder, stream->codecpar), "avcodec_parameters_to_context");
  bail_if(avcodec_open2(decoder, codec, NULL), "avcodec_open2 (audio)");
  if (!decoder->channel_layout) /* Is this needed ?*/
    decoder->channel_layout = av_get_default_channel_layout(decoder->channels);
  return new_input_container(demuxer, decoder, demuxer->streams[si]);
}

static double amp_scale(double a, int ascale){
  double min = 1e-6;
  switch(ascale) {
  case AS_SQRT:
    return 1.0 - sqrt(a);
  case AS_CBRT:
    return 1.0 - cbrt(a);
  case AS_LOG:
    return log(av_clipd(a, min, 1)) / log(min);
  case AS_LINEAR:
    return 1.0 - a;
  }
  return a;
}

/* https://stackoverflow.com/questions/14989397/how-to-convert-sample-rate-from-av-sample-fmt-fltp-to-av-sample-fmt-s16 */
static SwrContext *create_resampler(AVCodecContext *decoder, int64_t sample_rate, int64_t ch_layout){
  SwrContext *swr = swr_alloc_set_opts(NULL, ch_layout, AV_SAMPLE_FMT_FLTP, sample_rate,
    decoder->channel_layout, decoder->sample_fmt, decoder->sample_rate, 0, NULL);
  bail_if(swr_init(swr), "swr_init");
  return swr;
}

static SEXP run_fft(spectrum_container *output, int win_func, int ascale){
  AVPacket *pkt = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();
  input_container * input = output->audio_input;
  AVCodecContext *decoder = input->decoder;
  int fft_size = output->winsize;
  float overlap = output->overlap;
  int channels = output->keep_channels ? decoder->channels : 1;
  output->channels = channels;
  int fft_bits = av_log2(fft_size);
  int window_size = 1 << fft_bits;
  int hop_size = window_size * (1 - overlap);
  int output_range = window_size / 2;
  int iter = 0;
  output->swr = create_resampler(decoder, output->sample_rate,
                                 output->keep_channels ? decoder->channel_layout : AV_CH_LAYOUT_MONO);
  output->fft = av_fft_init(fft_bits, 0);
  output->fft_data = av_calloc(channels, sizeof(*output->fft_data));
  float scale = 0;
  output->src_data = av_calloc(channels, sizeof(*output->src_data));
  output->dst_data = av_calloc(channels, sizeof(*output->dst_data));
  for (int ch = 0; ch < channels; ch++) {
    output->fft_data[ch] = av_calloc(window_size, sizeof(**output->fft_data));
    output->src_data[ch] = av_calloc(window_size, sizeof(**output->src_data));
  }
  output->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 1, window_size);
  output->window_func_lut = av_realloc_f(NULL, window_size, sizeof(*output->window_func_lut));
  generate_window_func(output->window_func_lut, window_size, win_func, &overlap);
  for (int i = 0; i < window_size; i++) {
    scale += output->window_func_lut[i] * output->window_func_lut[i];
  }
  int eof = 0;
  while(!eof){
    while(!eof && av_audio_fifo_size(output->fifo) < window_size){
      int ret = avcodec_receive_frame(input->decoder, frame);
      if(ret == AVERROR(EAGAIN)){
        ret = av_read_frame(input->demuxer, pkt);
        if(ret == AVERROR_EOF){
          bail_if(avcodec_send_packet(input->decoder, NULL), "avcodec_send_packet (flush)");
        } else {
          bail_if(ret, "av_read_frame");
          if(pkt->stream_index == input->stream->index){
            av_packet_rescale_ts(pkt, input->stream->time_base, input->decoder->time_base);
            bail_if(avcodec_send_packet(input->decoder, pkt), "avcodec_send_packet (audio)");
            av_packet_unref(pkt);
          }
        }
      } else if(ret == AVERROR_EOF){
        eof = 1;
        break;
      } else {
        bail_if(ret, "avcodec_receive_frame");
        uint8_t *buf = NULL; /* https://ffmpeg.org/doxygen/3.2/group__lavu__sampmanip.html#ga4db4c77f928d32c7d8854732f50b8c04 */
        av_samples_alloc(&buf, NULL, output->channels, output->winsize, AV_SAMPLE_FMT_FLTP, 0);
        int out_samples = swr_convert (output->swr, &buf, output->winsize, (const uint8_t**) frame->extended_data, frame->nb_samples);
        av_frame_unref(frame);
        int nb_written = av_audio_fifo_write(output->fifo, (void **) &buf, out_samples);
        av_freep(&buf);
        bail_if(nb_written, "av_audio_fifo_write");
      }
      R_CheckUserInterrupt();
    }
    while ((av_audio_fifo_size(output->fifo) >= window_size) || (av_audio_fifo_size(output->fifo) > 0 && eof)) {
      int n_samples = av_audio_fifo_peek(output->fifo, (void**)output->src_data, window_size);
      bail_if(n_samples, "av_audio_fifo_peek");
      for (int ch = 0; ch < channels; ch++) {
        const float *src = output->src_data[ch];
        FFTComplex *fft_channel = output->fft_data[ch];
        int n;
        for (n = 0; n < n_samples; n++) {
          fft_channel[n].re = src[n] * output->window_func_lut[n];
          fft_channel[n].im = 0;
        }
        for (; n < window_size; n++) {
          fft_channel[n].re = 0;
          fft_channel[n].im = 0;
        }
      }

#define RE(x, ch) output->fft_data[ch][x].re
#define IM(x, ch) output->fft_data[ch][x].im
#define M(a, b) (sqrt((a) * (a) + (b) * (b)))

      for (int ch = 0; ch < channels; ch++) {
        FFTComplex *fft_channel = output->fft_data[ch];
        av_fft_permute(output->fft, fft_channel);
        av_fft_calc(output->fft, fft_channel);
      }

      for (int ch = 0; ch < channels; ch++) {
        output->dst_data[ch] = av_realloc_f(output->dst_data[ch], (iter+1) * output_range, sizeof(**output->dst_data));
        for (int n = 0; n < output_range; n++) {
          double a = M(RE(n, ch), IM(n, ch)) / scale;
          output->dst_data[ch][iter * output_range + n] = amp_scale(a, ascale);
        }
      }
      av_audio_fifo_drain(output->fifo, hop_size);
      R_CheckUserInterrupt();
      iter++;
    }
  }
  SEXP dims = PROTECT(Rf_allocVector(INTSXP, output->keep_channels ? 3 : 2));
  INTEGER(dims)[0] = output_range;
  INTEGER(dims)[1] = iter;
  if(output->keep_channels)
    INTEGER(dims)[2] = channels;
  SEXP out = PROTECT(Rf_allocVector(REALSXP, channels * iter * output_range));
  size_t n_slices = iter * output_range;
  for(int ch = 0; ch < channels; ch++){
    memcpy(REAL(out) + ch * n_slices, output->dst_data[ch], n_slices * sizeof(double));
  }
  Rf_setAttrib(out, R_DimSymbol, dims);
  UNPROTECT(2);
  return out;
}

static SEXP calculate_audio_fft(void *output){
  total_open_handles++;
  return run_fft(output, WFUNC_HANNING, AS_LOG);
}

SEXP R_audio_fft(SEXP audio, SEXP winsize, SEXP overlap, SEXP sample_rate, SEXP keep_channels){
  spectrum_container *output = av_mallocz(sizeof(spectrum_container));
  output->winsize = Rf_asInteger(winsize);
  output->overlap = Rf_asReal(overlap);
  output->keep_channels = Rf_asLogical(keep_channels);
  output->audio_input = open_audio_input(CHAR(STRING_ELT(audio, 0)));
  output->sample_rate = Rf_length(sample_rate) ? Rf_asInteger(sample_rate) :
    output->audio_input->decoder->sample_rate;
  return R_UnwindProtect(calculate_audio_fft, output, close_spectrum_container, output, NULL);
}
