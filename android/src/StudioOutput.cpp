#include "StudioOutput.h"
#include <QAudioSink>
#include <QIODevice>
#include <algorithm>
#include <cstring>
StudioOutput::StudioOutput(QObject *p):QObject(p),m_timer(new QTimer(this)) {
    m_timer->setInterval(5);m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer,&QTimer::timeout,this,&StudioOutput::pump);m_meterClock.start();
}
StudioOutput::~StudioOutput(){clear();}
void StudioOutput::clear(){
    m_timer->stop();m_io=nullptr;
    if(m_sink){m_sink->stop();delete m_sink;m_sink=nullptr;}
    for(auto &b:m_blocks)b.budget->queuedUs.fetch_sub(b.duration);
    m_blocks.clear();m_pendingUs=0;m_format={};m_finishing=false;
}
void StudioOutput::reset(quint64 token){clear();m_token=token;m_paused=false;}
void StudioOutput::fail(const QString &reason){const auto token=m_token;clear();emit failed(token,reason);}
void StudioOutput::volume(float v){m_volume=std::clamp(v,0.f,1.f);if(m_sink)m_sink->setVolume(m_volume);}
void StudioOutput::pause(bool value){m_paused=value;if(m_sink&&m_io){if(value)m_sink->suspend();else m_sink->resume();}}
void StudioOutput::enqueue(const QAudioBuffer &b,const QAudioDevice &device,std::shared_ptr<StudioBudget> budget,quint64 token){
    if(token!=m_token){budget->queuedUs.fetch_sub(b.duration());return;}
    const auto f=b.format();QAudioFormat target;target.setSampleRate(f.sampleRate());target.setChannelCount(f.channelCount());target.setSampleFormat(QAudioFormat::Float);
    if(m_sink&&target!=m_format){budget->queuedUs.fetch_sub(b.duration());fail(tr("Audio format changed; retry Studio"));return;}
    if(!m_sink){
        if(device.isNull()||!device.isFormatSupported(target)){budget->queuedUs.fetch_sub(b.duration());fail(tr("This output cannot use Studio at the source quality"));return;}
        m_format=target;m_dsp.configure(target.sampleRate(),target.channelCount());
        emit sourceFormat(m_token,target.sampleRate());
        m_sink=new QAudioSink(device,target,this);m_sink->setBufferSize(target.bytesForDuration(300000));m_sink->setVolume(m_volume);
        m_timer->start();
    }
    Block block;block.duration=b.duration();block.budget=std::move(budget);
    block.pcm.resize(qsizetype(b.frameCount())*f.channelCount()*sizeof(float));
    auto samples=reinterpret_cast<float*>(block.pcm.data());const char *input=b.constData<char>();
    for(qsizetype i=0;i<qsizetype(b.frameCount())*f.channelCount();++i){
        if(f.sampleFormat()==QAudioFormat::Float)memcpy(samples+i,input+i*4,4);
        else samples[i]=f.normalizedSampleValue(input+i*f.bytesPerSample());
    }
    m_dsp.process(samples,b.frameCount());m_pendingUs+=block.duration;m_blocks.push_back(std::move(block));
    if(m_meterClock.elapsed()>=50){emit meter(m_token,m_dsp.reductionDb());m_meterClock.restart();}
    pump();
}
void StudioOutput::pump(){
    if(!m_sink||m_paused)return;
    if(!m_io){
        if(!m_finishing&&m_pendingUs<220000)return; // Prime the output before playback.
        m_io=m_sink->start();if(!m_io){fail(tr("Studio audio output could not start"));return;}
    }
    if(m_sink->error()!=QtAudio::NoError){fail(tr("Studio audio output was interrupted"));return;}
    const qint64 frameBytes=m_format.bytesPerFrame();
    while(!m_blocks.empty()){
        auto &b=m_blocks.front();qint64 count=std::min<qint64>(b.pcm.size()-b.offset,m_sink->bytesFree());
        count-=count%frameBytes;if(!count)break;
        const auto written=m_io->write(b.pcm.constData()+b.offset,count);
        if(written<0){fail(tr("Studio audio output failed"));return;}if(!written)break;b.offset+=written;
        if(b.offset==b.pcm.size()){m_pendingUs-=b.duration;b.budget->queuedUs.fetch_sub(b.duration);m_blocks.pop_front();}
    }
    if(m_finishing){
        if(m_blocks.empty()&&m_sink->bytesFree()>=m_sink->bufferSize()){
            // Flush the fixed DSP lookahead tail through the same bounded path.
            const auto token=m_token;clear();emit drained(token);
        }else if(m_finishClock.elapsed()>4000)fail(tr("Studio audio output did not finish"));
    }
}
void StudioOutput::finish(quint64 token){
    if(token!=m_token)return;
    if(!m_sink){emit drained(token);return;}
    const int frames=m_dsp.latencyFrames();
    Block tail;tail.pcm.resize(frames*m_format.bytesPerFrame());tail.pcm.fill(0);m_dsp.process(reinterpret_cast<float*>(tail.pcm.data()),frames);
    tail.budget=std::make_shared<StudioBudget>();tail.duration=0;m_blocks.push_back(std::move(tail));
    m_finishing=true;m_finishClock.restart();pump();
}
