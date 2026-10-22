#include "SoundDsp.h"
#include <algorithm>
#include <cmath>
#include <complex>
namespace {
constexpr double Pi=3.14159265358979323846,Ceiling=0.891250938;
double frequency(int i){return i<10?SoundDsp::Frequencies[i]:i==10?90:i==11?1800:7000;}
double q(int i){return i<10?1.4:i==10?.55:i==11?.65:.55;}
}
SoundDsp::Filter SoundDsp::coefficients(double f,double gain,double quality,int rate){
    Filter b;if(f>=rate*.45)return b;
    const double a=std::pow(10.,gain/40),w=2*Pi*f/rate,alpha=std::sin(w)/(2*quality),a0=1+alpha/a,c=-2*std::cos(w);
    b.b0=(1+alpha*a)/a0;b.b1=c/a0;b.b2=(1-alpha*a)/a0;b.a1=c/a0;b.a2=(1-alpha/a)/a0;return b;
}
void SoundDsp::configure(int rate,int channels){
    m_rate=std::clamp(rate,8000,192000);m_channels=std::clamp(channels,1,2);m_delay=m_rate/200;
    m_dryEnergy=m_wetEnergy=0;m_compareGain=1;
    m_filters={};m_gains={};m_dry={};m_wet={};m_step=0;m_frame=0;m_head=m_tail=0;m_limiter=1;m_mix=m_target.original?0:1;
    updateHeadroom();m_preamp=m_targetPreamp;updateFilters();
}
void SoundDsp::setSettings(const Settings &s){
    m_target=s;const auto clean=[](double x,double bound){return std::isfinite(x)?std::clamp(x,-bound,bound):0.;};
    for(auto &g:m_target.bands)g=clean(g,9);
    m_target.bass=clean(s.bass,6);m_target.vocals=clean(s.vocals,6);m_target.treble=clean(s.treble,6);updateHeadroom();
}
std::array<double,96> SoundDsp::responseDb(const Settings &s,int rate){
    rate=std::clamp(rate,8000,192000);std::array<double,96> result{};
    std::array<Filter,13> filters;
    for(int i=0;i<13;++i)filters[i]=coefficients(frequency(i),i<10?s.bands[i]:i==10?s.bass:i==11?s.vocals:s.treble,q(i),rate);
    for(int bin=0;bin<96;++bin){
        const double f=20*std::pow(std::min(20000.,rate*.49)/20.,bin/95.);const auto z=std::polar(1.,-2*Pi*f/rate);double response=1;
        if(f>=rate*.5){result[bin]=0;continue;}
        for(const auto &b:filters)response*=std::abs((b.b0+b.b1*z+b.b2*z*z)/(1.+b.a1*z+b.a2*z*z));
        result[bin]=20*std::log10(std::max(response,1e-12));
    }return result;
}
double SoundDsp::requiredHeadroomDb(const Settings &s,int rate){
    rate=std::clamp(rate,8000,192000);std::array<Filter,13> filters;
    for(int i=0;i<13;++i)filters[i]=coefficients(frequency(i),i<10?s.bands[i]:i==10?s.bass:i==11?s.vocals:s.treble,q(i),rate);
    double peak=1;
    for(int bin=0;bin<512;++bin){
        const double f=20*std::pow((rate*.49)/20.,bin/511.);const auto z=std::polar(1.,-2*Pi*f/rate);double response=1;
        for(const auto &b:filters)response*=std::abs((b.b0+b.b1*z+b.b2*z*z)/(1.+b.a1*z+b.a2*z*z));
        peak=std::max(peak,response);
    }return 20*std::log10(peak);
}
void SoundDsp::updateHeadroom(){m_targetPreamp=std::pow(10.,-requiredHeadroomDb(m_target,m_rate)/20.);}
void SoundDsp::updateFilters(){
    const double smoothing=1-std::exp(-64./(.050*m_rate));
    for(int i=0;i<13;++i){
        const double target=i<10?m_target.bands[i]:i==10?m_target.bass:i==11?m_target.vocals:m_target.treble;
        m_gains[i]+=(target-m_gains[i])*smoothing;
        auto b=coefficients(frequency(i),m_gains[i],q(i),m_rate);
        b.z1=m_filters[i].z1;b.z2=m_filters[i].z2;m_filters[i]=b;
    }
}
void SoundDsp::process(float *samples,int frames){
    const double ramp=1-std::exp(-1./(.040*m_rate)),release=1-std::exp(-1./(.120*m_rate)),headroomAttack=1-std::exp(-1./(.005*m_rate));
    const double energyRate=1-std::exp(-1./(3.*m_rate)),compareRate=1-std::exp(-1./(.3*m_rate));
    for(int frame=0;frame<frames;++frame,++m_frame){
        if(m_step==0)updateFilters();m_step=(m_step+1)%64;
        m_mix+=((m_target.original?0.:1.)-m_mix)*ramp;
        // Attenuate quickly before increasing EQ gains; release headroom gently.
        m_preamp+=(m_targetPreamp-m_preamp)*(m_targetPreamp<m_preamp?headroomAttack:ramp);
        const int write=int(m_frame%Capacity);double peak=0;
        for(int ch=0;ch<m_channels;++ch){
            const float s=samples[frame*m_channels+ch];const double dry=std::isfinite(s)?double(s):0.;m_dry[write][ch]=dry;
            double value=dry*m_preamp;
            for(auto &b:m_filters){const double out=b.b0*value+b.z1[ch];b.z1[ch]=b.b1*value-b.a1*out+b.z2[ch];b.z2[ch]=b.b2*value-b.a2*out;value=out;}
            m_wet[write][ch]=std::isfinite(value)?value:0;peak=std::max(peak,std::abs(m_wet[write][ch]));
        }
        const auto outputFrame=m_frame-m_delay;
        while(m_head!=m_tail&&m_peaks[m_head].frame<outputFrame)m_head=(m_head+1)%Capacity;
        const double allowed=peak>Ceiling?Ceiling/peak:1.;
        const double key=allowed+double(m_frame)/m_delay;
        while(m_head!=m_tail){const int back=(m_tail+Capacity-1)%Capacity;if(m_peaks[back].key<key)break;m_tail=back;}
        m_peaks[m_tail]={m_frame,key};m_tail=(m_tail+1)%Capacity;
        const double gain=std::clamp(m_peaks[m_head].key-double(outputFrame)/m_delay,0.,1.);
        m_limiter=std::min(gain,m_limiter+(1-m_limiter)*release);
        const int read=outputFrame<0?0:int(outputFrame%Capacity);
        double dryEnergy=0,wetEnergy=0;
        if(outputFrame>=0)for(int ch=0;ch<m_channels;++ch){
            dryEnergy+=m_dry[read][ch]*m_dry[read][ch];
            wetEnergy+=std::pow(m_wet[read][ch]*m_limiter,2);
        }
        m_dryEnergy+=(dryEnergy/m_channels-m_dryEnergy)*energyRate;
        m_wetEnergy+=(wetEnergy/m_channels-m_wetEnergy)*energyRate;
        // Attenuate only the reference: never raise the processed signal to match.
        // This is a rolling RMS comparison, not a LUFS loudness measurement.
        const double match=m_dryEnergy>1e-8?std::clamp(std::sqrt(m_wetEnergy/m_dryEnergy),.063,1.):m_compareGain;
        const double target=m_target.levelMatch?match:1.;
        m_compareGain+=(target-m_compareGain)*compareRate;
        for(int ch=0;ch<m_channels;++ch)samples[frame*m_channels+ch]=outputFrame<0?0.f:float(m_dry[read][ch]*m_compareGain*(1-m_mix)+m_wet[read][ch]*m_limiter*m_mix);
    }
}
double SoundDsp::reductionDb()const{return -20*std::log10(std::max(m_limiter,1e-12));}
double SoundDsp::headroomDb()const{return -20*std::log10(std::max(m_targetPreamp,1e-12));}
