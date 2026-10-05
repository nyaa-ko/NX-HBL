#include <switch.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#include "../common/common.h"

#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"

#define SAMPLERATE 48000
#define CHANNELCOUNT 2
#define BYTESPERSAMPLE 2
#define AUDIO_BUF_MS 100

static s16 *g_pcm = NULL;
static size_t g_pcm_bytes = 0;

static s16 *g_click_pcm = NULL;
static size_t g_click_pcm_bytes = 0;
static volatile bool g_click_pending = false;

static u8 *g_raw[2] = { NULL, NULL };
static AudioOutBuffer g_source[2];

static Thread g_audio_thread;
static volatile bool g_audio_exit = false;

static bool g_audio_started = false;
static bool g_audout_inited = false;


/* ---------------------------------------------------------
 * Read entire file into memory
 * --------------------------------------------------------- */

static u8 *read_whole_file(const char *path, size_t *out_size)
{
    FILE *f;
    long sz;
    u8 *buf;

    *out_size = 0;

    f = fopen(path, "rb");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    sz = ftell(f);

    if (sz <= 0) {
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    buf = (u8 *)malloc((size_t)sz);

    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);

    *out_size = (size_t)sz;

    return buf;
}


/* ---------------------------------------------------------
 * Resample decoded OGG to 48 kHz stereo
 * --------------------------------------------------------- */

static s16 *resample_to_48k_stereo(
    const s16 *in,
    int in_ch,
    int in_rate,
    int in_frames,
    int *out_frames
)
{
    int oframes;
    s16 *out;
    int i;
    int c;

    float vol = HBL_BGM_VOLUME;

    if (in_rate <= 0 ||
        in_frames <= 0 ||
        in_ch <= 0 ||
        !in)
    {
        return NULL;
    }

    oframes =
        (int)((int64_t)in_frames *
              SAMPLERATE /
              in_rate);

    if (oframes < 1)
        oframes = 1;

    out = (s16 *)malloc(
        (size_t)oframes *
        CHANNELCOUNT *
        sizeof(s16)
    );

    if (!out)
        return NULL;

    for (i = 0; i < oframes; i++) {

        double src =
            (double)i *
            (double)in_rate /
            (double)SAMPLERATE;

        int i0 = (int)src;
        int i1 = i0 + 1;

        float t;
        float v;

        if (i0 >= in_frames)
            i0 = in_frames - 1;

        if (i1 >= in_frames)
            i1 = in_frames - 1;

        t = (float)(
            src -
            (double)i0
        );

        for (c = 0; c < CHANNELCOUNT; c++) {

            int sc;

            if (in_ch == 1)
                sc = 0;
            else if (c < in_ch)
                sc = c;
            else
                sc = in_ch - 1;

            float s0 =
                (float)in[
                    (size_t)i0 *
                    (size_t)in_ch +
                    (size_t)sc
                ];

            float s1 =
                (float)in[
                    (size_t)i1 *
                    (size_t)in_ch +
                    (size_t)sc
                ];

            v =
                (s0 + (s1 - s0) * t) *
                vol;

            if (v > 32767.0f)
                v = 32767.0f;

            if (v < -32768.0f)
                v = -32768.0f;

            out[
                (size_t)i *
                CHANNELCOUNT +
                (size_t)c
            ] = (s16)v;
        }
    }

    *out_frames = oframes;

    return out;
}


/* ---------------------------------------------------------
 * Fill BGM buffer
 *
 * Also mixes the pending UI click sound into the buffer.
 * button_click.pcm:
 *
 *   48000 Hz
 *   stereo
 *   signed 16-bit PCM
 *
 * --------------------------------------------------------- */

static void fill_pcm_buffer(
    AudioOutBuffer *buf,
    size_t *offset
)
{
    u8 *dst;
    size_t remain;
    size_t off;

    dst = (u8 *)buf->buffer;
    remain = buf->buffer_size;
    off = *offset;

    if (!g_pcm || g_pcm_bytes == 0) {

        memset(
            dst,
            0,
            remain
        );

        buf->data_size =
            buf->buffer_size;

        return;
    }

    /*
     * Loop BGM.
     */
    while (remain) {

        size_t chunk =
            g_pcm_bytes - off;

        if (chunk > remain)
            chunk = remain;

        memcpy(
            dst,
            ((u8 *)g_pcm) + off,
            chunk
        );

        dst += chunk;
        remain -= chunk;
        off += chunk;

        if (off >= g_pcm_bytes)
            off = 0;
    }

    /*
     * Mix click sound into the current
     * audio buffer.
     */
    if (g_click_pending &&
        g_click_pcm &&
        g_click_pcm_bytes > 0)
    {
        size_t click_samples =
            g_click_pcm_bytes /
            sizeof(s16);

        size_t output_samples =
            buf->buffer_size /
            sizeof(s16);

        size_t samples =
            click_samples;

        size_t i;

        s16 *out =
            (s16 *)buf->buffer;

        const s16 *click =
            g_click_pcm;

        if (samples > output_samples)
            samples = output_samples;

        for (i = 0; i < samples; i++) {

            int v =
                (int)out[i] +
                (int)click[i];

            if (v > 32767)
                v = 32767;

            if (v < -32768)
                v = -32768;

            out[i] = (s16)v;
        }

        g_click_pending = false;
    }

    buf->data_size =
        buf->buffer_size;

    *offset = off;
}


/* ---------------------------------------------------------
 * Audio playback thread
 * --------------------------------------------------------- */

static void audio_playback_thread(void *arg)
{
    size_t offset = 0;

    AudioOutBuffer *released = NULL;

    u32 released_count = 0;

    int i;

    (void)arg;

    /*
     * Queue initial buffers.
     */
    for (i = 0; i < 2; i++) {

        fill_pcm_buffer(
            &g_source[i],
            &offset
        );

        audoutAppendAudioOutBuffer(
            &g_source[i]
        );
    }

    /*
     * Keep replacing finished buffers.
     */
    while (!g_audio_exit) {

        Result rc =
            audoutWaitPlayFinish(
                &released,
                &released_count,
                100000000ULL
            );

        if (R_FAILED(rc) ||
            !released)
        {
            continue;
        }

        fill_pcm_buffer(
            released,
            &offset
        );

        audoutAppendAudioOutBuffer(
            released
        );
    }
}


/* ---------------------------------------------------------
 * Initialize audio
 * --------------------------------------------------------- */

void audio_initialize(void)
{
    Result rc = 0;

    u8 *ogg = NULL;
    size_t ogg_size = 0;

    s16 *decoded = NULL;

    int channels = 0;
    int rate = 0;
    int frames = 0;
    int out_frames = 0;

    u32 samples_per_buf;
    u32 raw_size;
    u32 raw_aligned;

    int i;

    g_audio_exit = false;
    g_audio_started = false;
    g_audout_inited = false;

    g_pcm = NULL;
    g_pcm_bytes = 0;

    g_click_pcm = NULL;
    g_click_pcm_bytes = 0;
    g_click_pending = false;


    /*
     * Load UI click sound.
     *
     * This is optional. If it cannot be loaded,
     * BGM can still work normally.
     */
    g_click_pcm =
        (s16 *)read_whole_file(
            "romfs:/button_click.pcm",
            &g_click_pcm_bytes
        );

    if (!g_click_pcm)
        g_click_pcm_bytes = 0;


    /*
     * Load BGM.
     */
    ogg =
        read_whole_file(
            "romfs:/bgMusic.ogg",
            &ogg_size
        );

    if (!ogg) {

        free(g_click_pcm);

        g_click_pcm = NULL;
        g_click_pcm_bytes = 0;

        return;
    }


    /*
     * Decode OGG.
     */
    frames =
        stb_vorbis_decode_memory(
            ogg,
            (int)ogg_size,
            &channels,
            &rate,
            &decoded
        );

    free(ogg);
    ogg = NULL;


    if (frames <= 0 ||
        !decoded ||
        channels <= 0)
    {
        free(decoded);

        free(g_click_pcm);

        g_click_pcm = NULL;
        g_click_pcm_bytes = 0;

        return;
    }


    /*
     * Convert to 48 kHz stereo.
     */
    g_pcm =
        resample_to_48k_stereo(
            decoded,
            channels,
            rate,
            frames,
            &out_frames
        );

    free(decoded);


    if (!g_pcm ||
        out_frames <= 0)
    {
        free(g_pcm);

        g_pcm = NULL;

        free(g_click_pcm);

        g_click_pcm = NULL;
        g_click_pcm_bytes = 0;

        return;
    }


    g_pcm_bytes =
        (size_t)out_frames *
        CHANNELCOUNT *
        sizeof(s16);


    /*
     * Calculate audio buffer size.
     */
    samples_per_buf =
        SAMPLERATE *
        AUDIO_BUF_MS /
        1000;

    raw_size =
        samples_per_buf *
        CHANNELCOUNT *
        BYTESPERSAMPLE;

    /*
     * libnx audio buffers need
     * suitable alignment.
     */
    raw_aligned =
        (raw_size + 0xfff) &
        ~0xfff;


    g_raw[0] =
        (u8 *)memalign(
            0x1000,
            raw_aligned
        );

    g_raw[1] =
        (u8 *)memalign(
            0x1000,
            raw_aligned
        );


    if (!g_raw[0] ||
        !g_raw[1])
    {
        free(g_raw[0]);
        free(g_raw[1]);

        g_raw[0] = NULL;
        g_raw[1] = NULL;

        free(g_pcm);

        g_pcm = NULL;
        g_pcm_bytes = 0;

        free(g_click_pcm);

        g_click_pcm = NULL;
        g_click_pcm_bytes = 0;

        return;
    }


    memset(
        g_raw[0],
        0,
        raw_aligned
    );

    memset(
        g_raw[1],
        0,
        raw_aligned
    );


    /*
     * Setup libnx AudioOut buffers.
     */
    for (i = 0; i < 2; i++) {

        memset(
            &g_source[i],
            0,
            sizeof(g_source[i])
        );

        g_source[i].buffer =
            g_raw[i];

        g_source[i].buffer_size =
            raw_size;

        g_source[i].data_size =
            raw_size;

        g_source[i].data_offset =
            0;
    }


    /*
     * Initialize audio output.
     */
    rc =
        audoutInitialize();

    if (R_SUCCEEDED(rc))
        rc =
            audoutStartAudioOut();


    if (R_FAILED(rc)) {

        audoutStopAudioOut();
        audoutExit();

        free(g_raw[0]);
        free(g_raw[1]);

        g_raw[0] = NULL;
        g_raw[1] = NULL;

        free(g_pcm);

        g_pcm = NULL;
        g_pcm_bytes = 0;

        free(g_click_pcm);

        g_click_pcm = NULL;
        g_click_pcm_bytes = 0;

        return;
    }


    g_audout_inited = true;


    /*
     * Start playback thread.
     */
    rc =
        threadCreate(
            &g_audio_thread,
            audio_playback_thread,
            NULL,
            NULL,
            0x4000,
            0x2C,
            -2
        );


    if (R_SUCCEEDED(rc))
        rc =
            threadStart(
                &g_audio_thread
            );


    if (R_SUCCEEDED(rc)) {

        g_audio_started = true;

    } else {

        /*
         * Thread creation failed.
         */
        threadClose(
            &g_audio_thread
        );

        audoutStopAudioOut();
        audoutExit();

        g_audout_inited = false;

        free(g_raw[0]);
        free(g_raw[1]);

        g_raw[0] = NULL;
        g_raw[1] = NULL;

        free(g_pcm);

        g_pcm = NULL;
        g_pcm_bytes = 0;

        free(g_click_pcm);

        g_click_pcm = NULL;
        g_click_pcm_bytes = 0;
    }
}


/* ---------------------------------------------------------
 * Play UI click sound
 *
 * Called from:
 *
 *   hbl_ui.c
 *   menu.c
 *
 * --------------------------------------------------------- */

void audioPlayClick(void)
{
    /*
     * Audio is not running yet.
     */
    if (!g_audio_started)
        return;

    /*
     * Click sound failed to load.
     */
    if (!g_click_pcm ||
        g_click_pcm_bytes == 0)
    {
        return;
    }

    /*
     * The audio thread will mix the
     * click into the next available
     * audio buffer.
     */
    g_click_pending = true;
}


/* ---------------------------------------------------------
 * Shutdown audio
 * --------------------------------------------------------- */

void audio_exit(void)
{
    /*
     * Stop playback thread.
     */
    if (g_audio_started) {

        g_audio_exit = true;

        threadWaitForExit(
            &g_audio_thread
        );

        threadClose(
            &g_audio_thread
        );

        g_audio_started = false;
    }


    /*
     * Stop audio output.
     */
    if (g_audout_inited) {

        audoutStopAudioOut();
        audoutExit();

        g_audout_inited = false;
    }


    /*
     * Free BGM.
     */
    free(g_pcm);

    g_pcm = NULL;
    g_pcm_bytes = 0;


    /*
     * Free click sound.
     */
    free(g_click_pcm);

    g_click_pcm = NULL;
    g_click_pcm_bytes = 0;

    g_click_pending = false;


    /*
     * Free audio buffers.
     */
    free(g_raw[0]);
    free(g_raw[1]);

    g_raw[0] = NULL;
    g_raw[1] = NULL;
}