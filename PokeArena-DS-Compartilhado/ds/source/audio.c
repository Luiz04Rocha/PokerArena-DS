#include <nds.h>
#include "audio.h"
static s8 samples[7][4096] __attribute__((aligned(4)));
static uint32_t lastSerial;
static int lastBeat=-1, leadChannel=-1, bassChannel=-1;
static volatile unsigned musicNotes;

static void updateMusic(const Game *g) {
    static const uint16_t melody[32]={
        587,698,880,0,784,698,587,440,
        466,587,698,880,784,698,587,0,
        698,880,1047,880,784,698,880,0,
        784,659,523,659,587,523,440,0
    };
    static const uint16_t bass[8]={147,220,117,175,175,262,131,196};
    int beat=(g->state==STATE_FIGHT && !g->introTicks && !g->muted)?(int)(g->frameCounter/7):-1;
    if(beat==lastBeat) return;
    if(leadChannel>=0) soundKill(leadChannel);
    leadChannel=-1;
    if(beat>=0 && melody[beat%32]) {
        leadChannel=soundPlayPSG(DutyCycle_12,melody[beat%32],11,57);
        if(leadChannel>=0) ++musicNotes;
    }
    if(beat<0 || lastBeat<0 || beat/4!=lastBeat/4) {
        if(bassChannel>=0) soundKill(bassChannel);
        bassChannel=beat<0?-1:soundPlayPSG(DutyCycle_25,bass[(beat/4)%8],7,71);
    }
    lastBeat=beat;
}
void audioInit(void) {
    const int frequencies[7]={660,180,950,330,520,780,140};
    for(int event=0;event<7;++event) {
        unsigned phase=0;
        for(int i=0;i<4096;++i) {
            int frequency=frequencies[event];
            if(event==4 || event==5) frequency+=i/1024*160;
            phase+=(unsigned)(frequency*65536/8192);
            int triangle=(int)((phase>>8)&255);
            triangle=triangle<128 ? triangle-64 : 191-triangle;
            samples[event][i]=(s8)(triangle*(4096-i)/4096);
        }
    }
    DC_FlushRange(samples,sizeof(samples));
    soundEnable();
}
void audioUpdate(const Game *g) {
    updateMusic(g);
    if(g->soundSerial==lastSerial) return;
    lastSerial=g->soundSerial;
    if(g->muted) return;
    static const uint8_t volume[7]={38,44,32,56,46,50,44};
    if(g->soundEvent>=1 && g->soundEvent<=7)
        soundPlaySample(samples[g->soundEvent-1],SoundFormat_8Bit,4096,8192,volume[g->soundEvent-1],64,false,0);
    else if(g->soundEvent==8 || g->soundEvent==9)
        soundPlaySample(samples[0],SoundFormat_8Bit,4096,g->soundEvent==8?16000:24000,31,64,false,0);
}
