/* This changed in ffmpeg 5.1, see https://github.com/FFmpeg/FFmpeg/commit/086a8048 */
#if LIBAVUTIL_VERSION_MAJOR >= 58 || (LIBAVUTIL_VERSION_MAJOR >= 57 && defined(FF_API_OLD_CHANNEL_LAYOUT))
#define NEW_CHANNEL_API
#endif

/* Old FFT API was marked deprecated in ffmpeg 6.1 */
#if LIBAVUTIL_VERSION_MAJOR > 58 || (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR >= 29)
#define NEW_FFT_TX_API
#endif
