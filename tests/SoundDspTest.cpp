#include "SoundDsp.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <limits>
#include <random>
#include <iostream>
constexpr double Pi=3.14159265358979323846;
std::vector<float> tone(double frequency,double amplitude=.1,int frames=96000){std::vector<float>s(frames*2);for(int i=0;i<frames;++i){s[i*2]=amplitude*std::sin(2*Pi*frequency*i/48000);s[i*2+1]=-s[i*2];}return s;}
double rms(const std::vector<float>&s){double sum=0;for(size_t i=s.size()/2;i<s.size();++i)sum+=s[i]*s[i];return std::sqrt(sum/(s.size()/2));}
int main(){
    SoundDsp d;d.configure(48000,2);auto dry=tone(1000),flat=dry;d.process(flat.data(),flat.size()/2);
    assert(std::abs(rms(flat)/rms(dry)-1)<1e-6);
    SoundDsp::Settings eq;eq.bands[5]=6;d.setSettings(eq);auto boost=dry;d.process(boost.data(),boost.size()/2);
    const double expectedDb=6-d.headroomDb();assert(std::abs(20*std::log10(rms(boost)/rms(dry))-expectedDb)<.15);
    auto low=tone(63);SoundDsp lowDsp;lowDsp.setSettings(eq);lowDsp.configure(48000,2);auto lowDry=low;lowDsp.process(low.data(),low.size()/2);
    assert(20*std::log10((rms(boost)/rms(dry))/(rms(low)/rms(lowDry)))>5.5);
    eq.bands[5]=-6;d.setSettings(eq);auto cut=dry;d.process(cut.data(),cut.size()/2);assert(std::abs(20*std::log10(rms(cut)/rms(dry))+6)<.15);
    eq.bands.fill(9);eq.bass=eq.vocals=eq.treble=6;d.setSettings(eq);auto loud=tone(1000,.9);d.process(loud.data(),loud.size()/2);
    for(size_t i=0;i<loud.size();i+=2){assert(std::isfinite(loud[i]));assert(std::abs(loud[i])<.89126);assert(std::abs(loud[i]+loud[i+1])<1e-6);}
    eq.original=true;d.setSettings(eq);d.configure(48000,2);auto original=tone(1000,1.2),copy=original;d.process(copy.data(),copy.size()/2);
    for(size_t i=0;i<copy.size();++i)assert(copy[i]==(i<size_t(d.latencyFrames()*2)?0.f:original[i-d.latencyFrames()*2]));
    eq.original=false;SoundDsp a,b;a.setSettings(eq);b.setSettings(eq);a.configure(48000,2);b.configure(48000,2);auto one=dry,chunked=dry;a.process(one.data(),one.size()/2);
    for(int i=0;i<int(chunked.size()/2);i+=137)b.process(chunked.data()+i*2,std::min(137,int(chunked.size()/2)-i));assert(one==chunked);
    // Loud, irregular peaks at multiple rates must remain bounded without a hard sample clip.
    std::mt19937 random(42);std::uniform_real_distribution<float> noise(-2,2);
    for(int rate:{8000,44100,48000,96000,192000}){
        SoundDsp limiter;limiter.configure(rate,2);std::vector<float> input(rate*2);for(auto &x:input)x=noise(random);limiter.process(input.data(),rate);
        for(float x:input)assert(std::isfinite(x)&&std::abs(x)<=.89127);
    }
    // Preset transitions on a continuous tone must not create discontinuity spikes.
    SoundDsp changing;changing.configure(48000,2);double largestJump=0;float previous=0;
    for(int pass=0;pass<20;++pass){SoundDsp::Settings s;s.bands[pass%10]=pass%2?9:-9;changing.setSettings(s);auto block=tone(1000,.2,24000);changing.process(block.data(),24000);
        for(size_t i=0;i<block.size();i+=2){largestJump=std::max(largestJump,std::abs(double(block[i]-previous)));previous=block[i];}}
    assert(largestJump<.08);
    // The curve shown to users is the actual combined filter response.
    SoundDsp::Settings profile;profile.bass=2;profile.vocals=1.2;profile.treble=.7;profile.bands[3]=-.8;
    const auto curve=SoundDsp::responseDb(profile);
    for(int bin:{10,25,45,65,85}){
        const double frequency=20*std::pow(1000.,bin/95.);SoundDsp probe;probe.setSettings(profile);probe.configure(48000,2);
        auto signal=tone(frequency,.05,240000),source=signal;probe.process(signal.data(),signal.size()/2);
        const double measured=20*std::log10(rms(signal)/rms(source))+probe.headroomDb();
        assert(std::abs(measured-curve[bin])<.06);
    }
    // RMS-matched reference settles close to enhanced level; disabling restores dry gain.
    SoundDsp comparison;profile={};profile.bands[5]=-6;profile.levelMatch=true;
    comparison.setSettings(profile);comparison.configure(48000,2);auto enhanced=tone(1000,.1,480000);comparison.process(enhanced.data(),enhanced.size()/2);
    profile.original=true;comparison.setSettings(profile);auto reference=tone(1000,.1,480000);comparison.process(reference.data(),reference.size()/2);
    assert(std::abs(20*std::log10(rms(reference)/rms(enhanced)))<.05);
    profile.levelMatch=false;comparison.setSettings(profile);auto untouched=tone(1000,.1,240000),untouchedDry=untouched;comparison.process(untouched.data(),untouched.size()/2);
    assert(std::abs(rms(untouched)/rms(untouchedDry)-1)<.001);
    // Silence remains finite with the comparison estimator active.
    profile.levelMatch=true;profile.original=false;comparison.setSettings(profile);std::vector<float> silence(48000*2);comparison.process(silence.data(),48000);
    for(float x:silence)assert(std::isfinite(x));
    float invalid[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),0,0};a.configure(8000,2);a.process(invalid,2);for(float x:invalid)assert(std::isfinite(x));
    std::cout<<"PASS: Flat transparency, EQ shape/headroom, stereo lookahead limiting at 5 rates, Original alignment, segmentation, preset continuity\n";
}
