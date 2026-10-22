#pragma once
#include <array>
#include <cstdint>
class SoundDsp {
public:
    static constexpr int BandCount=10;
    static constexpr std::array<double,BandCount> Frequencies{31.5,63,125,250,500,1000,2000,4000,8000,16000};
    struct Settings {std::array<double,BandCount> bands{};double bass=0,vocals=0,treble=0;bool original=false;bool levelMatch=false;};
    void configure(int rate,int channels);
    void setSettings(const Settings &);
    void process(float *samples,int frames);
    double reductionDb()const;
    int latencyFrames()const{return m_delay;}
    double headroomDb()const;
    static std::array<double,96> responseDb(const Settings &,int rate=48000);
    static double requiredHeadroomDb(const Settings &,int rate=48000);
private:
    struct Filter {double b0=1,b1=0,b2=0,a1=0,a2=0;std::array<double,2> z1{},z2{};};
    static Filter coefficients(double frequency,double gain,double q,int rate);
    void updateFilters();void updateHeadroom();
    int m_rate=48000,m_channels=2,m_step=0,m_delay=240;
    Settings m_target;
    std::array<double,13> m_gains{};
    std::array<Filter,13> m_filters{};
    static constexpr int Capacity=2048;
    std::array<std::array<double,2>,Capacity> m_dry{},m_wet{};
    struct Peak {std::int64_t frame=0;double key=0;};
    std::array<Peak,Capacity> m_peaks{};
    int m_head=0,m_tail=0;std::int64_t m_frame=0;
    double m_dryEnergy=0,m_wetEnergy=0,m_compareGain=1;
    double m_mix=1,m_limiter=1,m_preamp=1,m_targetPreamp=1;
};
