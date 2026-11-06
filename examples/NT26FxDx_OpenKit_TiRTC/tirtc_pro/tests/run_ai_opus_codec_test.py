#!/usr/bin/env python3
"""Exercise Pro's actual Opus adapter with the repository's Opus 1.4 sources.

Builds the same generic fixed-point sources/options as the SDK's Opus Makefile.
Only logging is stubbed; encoder, decoder and packet parser are real. Host tests
cover packet sizes, lifecycle and buffer bounds, not board CPU or acoustic AEC.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

PROGRAM = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "opus.h"
#include "opus_codec.h"
static unsigned cases;
static struct {uint8_t before[16],data[DEMO_AI_OPUS_MAX_PACKET],after[16];} packet;
static struct {uint16_t before[8];int16_t data[DEMO_AI_OPUS_MAX_DECODE_SAMPLES];uint16_t after[8];} pcm;
static int16_t input[320],saved[1920];
static void guards(void) {
    unsigned i;
    for(i=0;i<16;++i)assert(packet.before[i]==0xa5 && packet.after[i]==0xa5);
    for(i=0;i<8;++i)assert(pcm.before[i]==0x6a6a && pcm.after[i]==0x6a6a);
}
static void fill_input(unsigned frame) {
    unsigned i;for(i=0;i<320;++i)input[i]=(int16_t)(((int)((i+frame*320U)%80U)-40)*200);
}
static void roundtrip(void) {
    unsigned i,k;int bytes, samples;long long energy=0;
    for(k=0;k<100;++k) {
        memset(&packet,0xa5,sizeof(packet));memset(&pcm,0x6a,sizeof(pcm));fill_input(k);
        bytes=demo_ai_opus_encode_20ms(input,packet.data,sizeof(packet.data));
        assert(bytes>0 && bytes<=DEMO_AI_OPUS_MAX_PACKET);
        assert(demo_ai_opus_packet_samples(packet.data,bytes)==320);
        samples=demo_ai_opus_decode(packet.data,bytes,pcm.data,320);assert(samples==320);
        guards();for(i=320;i<1920;++i)assert(pcm.data[i]==0x6a6a);
        for(i=0;i<320;++i)energy+=(long long)pcm.data[i]*pcm.data[i];
    }
    assert(energy>0);++cases;
}
static void reset_cases(void) {
    unsigned i;int bytes,again;uint8_t expected[1275];
    fill_input(0);demo_ai_opus_reset();
    bytes=demo_ai_opus_encode_20ms(input,packet.data,sizeof(packet.data));assert(bytes>0);
    memcpy(expected,packet.data,bytes);
    demo_ai_opus_reset_decoder();
    assert(demo_ai_opus_decode(packet.data,bytes,saved,320)==320);
    demo_ai_opus_reset_decoder();
    assert(demo_ai_opus_decode(packet.data,bytes,pcm.data,320)==320);
    assert(memcmp(saved,pcm.data,320*sizeof(int16_t))==0);++cases;
    demo_ai_opus_reset();again=demo_ai_opus_encode_20ms(input,packet.data,sizeof(packet.data));
    assert(again==bytes && memcmp(expected,packet.data,bytes)==0);++cases;
    for(i=0;i<8;++i) {
        demo_ai_opus_deinit();demo_ai_opus_deinit();demo_ai_opus_reset();demo_ai_opus_reset_decoder();
        assert(demo_ai_opus_encode_20ms(input,packet.data,1275)<0);
        assert(demo_ai_opus_decode(expected,bytes,pcm.data,320)<0);
        assert(demo_ai_opus_init()==0 && demo_ai_opus_init()==0);
        assert(demo_ai_opus_encode_20ms(input,packet.data,1275)>0);
    }
    ++cases;
}
static void long_packet(void) {
    uint8_t frames[6][1275];unsigned i;int error,bytes[6],total;
    OpusEncoder *enc=opus_encoder_create(16000,1,OPUS_APPLICATION_RESTRICTED_LOWDELAY,&error);
    OpusRepacketizer *rp=opus_repacketizer_create();assert(enc && error==OPUS_OK && rp);
    assert(opus_encoder_ctl(enc,OPUS_SET_BITRATE(24000))==OPUS_OK);
    assert(opus_encoder_ctl(enc,OPUS_SET_COMPLEXITY(3))==OPUS_OK);
    assert(opus_encoder_ctl(enc,OPUS_SET_VBR(0))==OPUS_OK);
    assert(opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND))==OPUS_OK);
    for(i=0;i<6;++i) {
        fill_input(i);bytes[i]=opus_encode(enc,input,320,frames[i],1275);assert(bytes[i]>0);
        assert(opus_repacketizer_cat(rp,frames[i],bytes[i])==OPUS_OK);
    }
    memset(&packet,0xa5,sizeof(packet));memset(&pcm,0x6a,sizeof(pcm));
    total=opus_repacketizer_out(rp,packet.data,1275);assert(total>0 && total<=1275);
    assert(demo_ai_opus_packet_samples(packet.data,total)==1920);
    demo_ai_opus_reset_decoder();
    assert(demo_ai_opus_decode(packet.data,total,pcm.data,1920)==1920);guards();++cases;
    memset(&pcm,0x6a,sizeof(pcm));demo_ai_opus_reset_decoder();
    assert(demo_ai_opus_decode(packet.data,total,pcm.data,320)==OPUS_BUFFER_TOO_SMALL);
    guards();for(i=320;i<1920;++i)assert(pcm.data[i]==0x6a6a);++cases;
    opus_repacketizer_destroy(rp);opus_encoder_destroy(enc);
}
static void invalid_inputs(void) {
    uint8_t bad[2]={0xff,0xff};
    assert(demo_ai_opus_packet_samples(NULL,2)<0);
    assert(demo_ai_opus_packet_samples(bad,0)<0);
    assert(demo_ai_opus_packet_samples(bad,1276)<0);
    assert(demo_ai_opus_packet_samples(bad,2)<0);++cases;
    assert(demo_ai_opus_encode_20ms(NULL,packet.data,1275)<0);
    assert(demo_ai_opus_encode_20ms(input,NULL,1275)<0);
    assert(demo_ai_opus_encode_20ms(input,packet.data,0)<0);++cases;
    memset(&packet,0xa5,sizeof(packet));memset(&pcm,0x6a,sizeof(pcm));
    assert(demo_ai_opus_decode(NULL,2,pcm.data,1920)<0);
    assert(demo_ai_opus_decode(bad,2,NULL,1920)<0);
    assert(demo_ai_opus_decode(bad,0,pcm.data,1920)<0);
    assert(demo_ai_opus_decode(bad,2,pcm.data,0)<0);
    assert(demo_ai_opus_decode(bad,2,pcm.data,1920)<0);guards();++cases;
}
int main(void) {
    assert(demo_ai_opus_init()==0 && demo_ai_opus_init()==0);
    roundtrip();reset_cases();long_packet();invalid_inputs();demo_ai_opus_deinit();
    printf("PASS: Pro real Opus 1.4 fixed-point: %u cases, 100 roundtrip frames, 120ms packet, resets, invalid inputs, canaries\n",cases);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'), help='C compiler executable, including Zig')
    args = parser.parse_args()
    cc = [args.cc] + (['cc'] if Path(args.cc).stem.lower() == 'zig' else [])
    app = Path(__file__).resolve().parents[1]
    sdk = app.parents[2]
    opus_root = sdk / 'components/thirdparty/opus'
    opus = opus_root / 'opus-1.4'
    # Match components/thirdparty/opus/Makefile, without platform-specific SIMD.
    dirs = [opus / x for x in ('src', 'celt', 'silk', 'silk/fixed')] + [opus_root]
    excluded = {'repacketizer_demo.c', 'opus_demo.c', 'opus_custom_demo.c', 'opus_compare.c'}
    sources = sorted(p for d in dirs for p in d.glob('*.c') if p.name not in excluded)
    assert sources and len(sources) > 100
    with tempfile.TemporaryDirectory(prefix='pro-real-opus-') as temp:
        temp = Path(temp)
        (temp / 'liot_log.h').write_text('#define liot_trace(...) ((void)0)\n', encoding='utf-8')
        source = temp / 'test.c'
        source.write_text(PROGRAM, encoding='utf-8')
        exe = temp / ('test.exe' if os.name == 'nt' else 'test')
        includes = [temp, app / 'media', opus / 'include', *dirs[:-1]]
        flags = ['-std=c99', '-O1', '-DOPUS_BUILD', '-DHAVE_LRINT', '-DFIXED_POINT=1',
                 '-DDISABLE_FLOAT_API', '-DVAR_ARRAYS', *['-I' + str(p) for p in includes],
                 str(source), str(app / 'media/opus_codec.c'), *map(str, sources), '-o', str(exe), '-lm']
        # A response file avoids the Windows command-line length limit.
        response = temp / 'compile.rsp'
        response.write_text('\n'.join('"' + x.replace('\\', '/') + '"' for x in flags), encoding='utf-8')
        subprocess.run([*cc, '@' + str(response)], check=True, timeout=180)
        subprocess.run([str(exe)], check=True, timeout=30)


if __name__ == '__main__':
    main()
