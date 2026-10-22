#include "SoundStudio.h"
#include "AudioVisualizer.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QAudioSink>
#include <QAudioBuffer>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QIODevice>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <QUuid>
namespace {
double clean(double v,double limit){return std::isfinite(v)?std::clamp(v,-limit,limit):0.;}
QVariantList list(const std::array<double,10>& a){QVariantList v;for(double x:a)v.append(x);return v;}
}
namespace {
struct Profile {const char *id;const char *description;std::array<double,10> bands;double bass,vocals,treble;};
// Broad tone controls supply the main character; narrow bands only refine it.
// These are listening starting points, not device calibration curves.
const Profile builtins[]={
    {"Flat","Neutral equalizer. No tonal adjustment.",{},0,0,0},
    {"Balanced","Gentle warmth and definition for everyday listening.",{0,0,0,-.5,-.5,0,0,0,0,0},.8,.4,.5},
    {"Bollywood","Rhythmic warmth with clear singing and restrained highs.",{-.5,0,0,-.8,-.5,0,.3,-.5,0,-.5},2,1.2,.7},
    {"Ghazal","Natural voices with less low-mid masking and softer highs.",{-.5,0,0,-1,-.8,0,.2,0,-.5,-.5},-.5,1.8,-.7},
    {"Classical","Subtle definition while retaining orchestral dynamics.",{0,0,0,-.3,0,0,0,0,.3,0},0,.3,.7},
    {"Acoustic","Natural string detail with controlled warmth.",{-.5,0,0,-.5,-.5,.3,.3,-.3,0,0},.3,1,1.2},
    {"Dance","Controlled bass punch with space for vocals.",{0,.5,0,-1,-1,0,0,-.3,0,-.5},3,.5,1.5},
    {"Speech","Less rumble and clearer spoken words.",{-1,-1,0,-.8,-.5,.3,.5,-.5,-1,-1},-2,2,-.5}
};
}
SoundStudio::SoundStudio(QObject *p,const QString &file,bool persistent):QObject(p),m_persistent(persistent) {
    const QString path=file.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/sound-studio.ini":file;
    QDir().mkpath(QFileInfo(path).absolutePath());m_settings=std::make_unique<QSettings>(path,QSettings::IniFormat);
    m_enabled=m_settings->value("enabled",false).toBool();
    const auto values=m_settings->value("bands").toList();
    for(int i=0;i<std::min(10,int(values.size()));++i)m_values.bands[i]=clean(values[i].toDouble(),9);
    m_values.bass=clean(m_settings->value("bass",0).toDouble(),6);
    m_values.vocals=clean(m_settings->value("vocals",0).toDouble(),6);
    m_values.treble=clean(m_settings->value("treble",0).toDouble(),6);
    m_preset=m_settings->value("preset","Flat").toString();
    m_basePreset=m_settings->value("basePreset",m_preset).toString();
    m_strength=std::clamp(clean(m_settings->value("strength",100).toDouble(),100),0.,100.);
    m_values.levelMatch=m_settings->value("levelMatch",true).toBool();
    m_profiles=m_settings->value("profiles").toList();
    if(!m_settings->contains("profiles")&&m_settings->value("savedBands").toList().size()==10){
        m_profiles.append(QVariantMap{{"id","personal:legacy"},{"name",tr("My saved sound")},
            {"bands",m_settings->value("savedBands")},{"bass",m_settings->value("savedBass",0)},
            {"vocals",m_settings->value("savedVocals",0)},{"treble",m_settings->value("savedTreble",0)},{"strength",100}});
    }
    if(m_preset=="Saved"&&!m_profiles.isEmpty())m_preset=m_basePreset=m_profiles.first().toMap()["id"].toString();
    if(m_settings->value("profileSchema",0).toInt()<1){
        for(const auto &p:builtins)if(m_preset==p.id){m_values.bands=p.bands;m_values.bass=p.bass;m_values.vocals=p.vocals;m_values.treble=p.treble;m_strength=100;break;}
    }
    if(m_basePreset=="Custom")m_basePreset="Flat";
    m_save.setSingleShot(true);m_save.setInterval(300);connect(&m_save,&QTimer::timeout,this,&SoundStudio::save);
    m_worker=new StudioOutput;m_worker->moveToThread(&m_audioThread);
    connect(&m_audioThread,&QThread::finished,m_worker,&QObject::deleteLater);
    connect(m_worker,&StudioOutput::failed,this,[this](quint64 token,const QString &reason){if(token==m_token)fail(reason);});
    connect(m_worker,&StudioOutput::meter,this,[this](quint64 token,double value){if(token==m_token){m_reduction=value;emit meterChanged();}});
    connect(m_worker,&StudioOutput::sourceFormat,this,[this](quint64 token,int rate){
        if(token==m_token&&m_sampleRate!=rate){m_sampleRate=rate;emit settingsChanged();}
    });
    connect(m_worker,&StudioOutput::drained,this,[this](quint64 token){if(token==m_token){m_finishing=false;emit playbackFinished();}});
    m_audioThread.setObjectName("Jatayu audio");m_audioThread.start(QThread::HighPriority);
    const auto initial=effective();QMetaObject::invokeMethod(m_worker,[this,initial]{m_worker->settings(initial);});
    m_watch.setInterval(250);connect(&m_watch,&QTimer::timeout,this,[this]{
        if(m_routing && m_player && m_player->playbackState()==QMediaPlayer::PlayingState && m_player->hasAudio()
           && m_player->mediaStatus()==QMediaPlayer::BufferedMedia && studioClockMs()-m_budget->lastFeedMs.load()>2000)
            fail(tr("Studio is unavailable for this playback"));
    });
    auto devices=new QMediaDevices(this);connect(devices,&QMediaDevices::audioOutputsChanged,this,[this]{
        if(m_routing)fail(tr("Audio device changed; toggle Studio to retry"));
    });
}
SoundStudio::~SoundStudio(){m_save.stop();save();m_enabled=false;updateRouting();stopSink();m_audioThread.quit();m_audioThread.wait();}
QVariantList SoundStudio::bands()const{return list(m_values.bands);}
QString SoundStudio::status()const {
    if(!m_enabled)return tr("Original sound • Studio is off");
    if(!m_failure.isEmpty())return tr("Original sound • ")+m_failure;
    if(!m_music)return tr("Original sound • Studio is for music");
    if(m_player && !qFuzzyCompare(m_player->playbackRate(),1.))return tr("Original sound • Studio works at 1.0× speed");
    if(!m_routing)return tr("Original sound • Choose a song to start");
    return m_values.original?tr("Original sound • Compare with your settings"):tr("Enhanced sound • EQ and limiter active");
}
void SoundStudio::changed(){const auto values=effective();QMetaObject::invokeMethod(m_worker,[this,values]{m_worker->settings(values);});if(m_persistent)m_save.start();emit settingsChanged();emit stateChanged();}
void SoundStudio::copyConfigurationFrom(const SoundStudio &other){
    m_enabled=other.m_enabled;m_values=other.m_values;m_preset=other.m_preset;m_basePreset=other.m_basePreset;m_strength=other.m_strength;m_profiles=other.m_profiles;updateRouting();changed();
}
void SoundStudio::save(){
    if(!m_persistent)return;
    m_settings->setValue("enabled",m_enabled);m_settings->setValue("bands",bands());m_settings->setValue("bass",bass());
    m_settings->setValue("vocals",vocals());m_settings->setValue("treble",treble());m_settings->setValue("preset",m_preset);
    m_settings->setValue("basePreset",m_basePreset);m_settings->setValue("strength",m_strength);
    m_settings->setValue("profileSchema",1);m_settings->setValue("levelMatch",m_values.levelMatch);m_settings->setValue("profiles",m_profiles);
    m_settings->sync();QFile::setPermissions(m_settings->fileName(),QFileDevice::ReadOwner|QFileDevice::WriteOwner);
}
void SoundStudio::setEnabled(bool v){if(m_enabled==v)return;m_enabled=v;m_failure.clear();updateRouting();changed();}
void SoundStudio::setOriginal(bool v){if(m_values.original==v)return;m_values.original=v;changed();}
void SoundStudio::setMusic(bool v){if(m_music==v)return;m_music=v;m_failure.clear();updateRouting();emit stateChanged();}
void SoundStudio::setBass(double v){m_values.bass=clean(v,6);m_preset="Custom";changed();}
void SoundStudio::setVocals(double v){m_values.vocals=clean(v,6);m_preset="Custom";changed();}
void SoundStudio::setTreble(double v){m_values.treble=clean(v,6);m_preset="Custom";changed();}
void SoundStudio::setBand(int i,double v){if(i<0||i>=10)return;m_values.bands[i]=clean(v,9);m_preset="Custom";changed();}
SoundDsp::Settings SoundStudio::effective()const{
    auto s=m_values;const double scale=m_strength/100.;
    for(auto &v:s.bands)v*=scale;s.bass*=scale;s.vocals*=scale;s.treble*=scale;return s;
}
QVariantList SoundStudio::profiles()const{
    QVariantList result;for(const auto &p:builtins)result.append(QVariantMap{{"id",p.id},{"name",tr(p.id)}});
    for(const auto &v:m_profiles){const auto p=v.toMap();result.append(QVariantMap{{"id",p["id"]},{"name",p["name"]}});}return result;
}
QString SoundStudio::description()const{
    if(m_preset=="Custom")return tr("Your edited sound. Save it as a personal profile.");
    for(const auto &p:builtins)if(m_preset==p.id)return tr(p.description);
    return tr("Your personal sound profile.");
}
QVariantList SoundStudio::response()const{QVariantList result;for(double db:SoundDsp::responseDb(effective(),m_sampleRate))result.append(db);return result;}
double SoundStudio::headroom()const{return SoundDsp::requiredHeadroomDb(effective(),m_sampleRate);}
void SoundStudio::setStrength(double value){if(!std::isfinite(value))return;value=std::clamp(value,0.,100.);if(value==m_strength)return;m_strength=value;changed();}
void SoundStudio::setLevelMatch(bool value){if(value==m_values.levelMatch)return;m_values.levelMatch=value;changed();}
void SoundStudio::applyPreset(const QString &requested){
    QString name=requested;
    if(name=="Saved"&&!m_profiles.isEmpty())name=m_profiles.first().toMap()["id"].toString();
    for(const auto &p:builtins)if(name==p.id){
        m_values.bands=p.bands;m_values.bass=p.bass;m_values.vocals=p.vocals;m_values.treble=p.treble;
        m_preset=m_basePreset=name;m_strength=100;changed();return;
    }
    for(const auto &v:m_profiles){const auto p=v.toMap();if(p["id"].toString()!=name)continue;
        const auto b=p["bands"].toList();if(b.size()!=10)return;
        for(int i=0;i<10;++i)m_values.bands[i]=clean(b[i].toDouble(),9);
        m_values.bass=clean(p["bass"].toDouble(),6);m_values.vocals=clean(p["vocals"].toDouble(),6);m_values.treble=clean(p["treble"].toDouble(),6);
        m_strength=std::clamp(clean(p.value("strength",100).toDouble(),100),0.,100.);
        m_preset=m_basePreset=name;changed();return;
    }
}
void SoundStudio::resetPreset(){applyPreset(m_basePreset);}
bool SoundStudio::saveProfile(const QString &requested){
    const auto name=requested.trimmed().left(40);if(name.isEmpty()||m_profiles.size()>=32)return false;
    const auto id="personal:"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_profiles.append(QVariantMap{{"id",id},{"name",name},{"bands",bands()},{"bass",bass()},
        {"vocals",vocals()},{"treble",treble()},{"strength",m_strength}});
    m_preset=m_basePreset=id;changed();return true;
}
void SoundStudio::removeProfile(const QString &id){
    for(int i=0;i<m_profiles.size();++i)if(m_profiles[i].toMap()["id"].toString()==id){
        m_profiles.removeAt(i);if(m_basePreset==id){m_basePreset="Flat";m_preset="Custom";}changed();return;
    }
}
void SoundStudio::saveCustom(){saveProfile(tr("My sound"));}
void SoundStudio::attach(QObject *object,QObject *visualizer){
    const bool wasEnabled=m_enabled;
    m_enabled=false; // Restore the previous output before changing player ownership.
    updateRouting();
    if(m_player)disconnect(m_player,nullptr,this,nullptr);
    if(m_visualizer){m_visualizer->setCaptureAudio(false);disconnect(m_visualizer,nullptr,this,nullptr);}
    if(m_originalOutput)disconnect(m_originalOutput,nullptr,this,nullptr);
    m_player=qobject_cast<QMediaPlayer*>(object);m_visualizer=qobject_cast<AudioVisualizer*>(visualizer);
    m_originalOutput=m_player?m_player->audioOutput():nullptr;m_enabled=wasEnabled;
    m_userMuted=m_originalOutput&&m_originalOutput->isMuted();
    if(m_originalOutput){
        connect(m_originalOutput,&QAudioOutput::volumeChanged,this,&SoundStudio::syncVolume);
        connect(m_originalOutput,&QAudioOutput::mutedChanged,this,[this]{
            if(m_internalMute)return;
            m_userMuted=m_originalOutput->isMuted();
            if(m_routing){m_internalMute=true;m_originalOutput->setMuted(true);m_internalMute=false;}
            syncVolume();
        });
        connect(m_originalOutput,&QAudioOutput::deviceChanged,this,[this]{if(m_routing)fail(tr("Audio device changed; toggle Studio to retry"));});
    }
    if(m_player){
        connect(m_player,&QMediaPlayer::sourceChanged,this,[this]{stopSink();m_failure.clear();updateRouting();});
        connect(m_player,&QMediaPlayer::hasVideoChanged,this,[this]{updateRouting();emit stateChanged();});
        connect(m_player,&QMediaPlayer::playbackRateChanged,this,[this]{updateRouting();emit stateChanged();});
        connect(m_player,&QMediaPlayer::mediaStatusChanged,this,[this]{m_budget->lastFeedMs.store(studioClockMs());});
        connect(m_player,&QMediaPlayer::playbackStateChanged,this,[this](QMediaPlayer::PlaybackState state){
            if(state==QMediaPlayer::PlayingState){m_budget->lastFeedMs.store(studioClockMs());if(m_budget->seekTarget.load()>=0)m_budget->seekStartedMs.store(studioClockMs());m_budget->paused.store(false);QMetaObject::invokeMethod(m_worker,[this]{m_worker->pause(false);});}
            else if(state==QMediaPlayer::PausedState){m_budget->paused.store(true);QMetaObject::invokeMethod(m_worker,[this]{m_worker->pause(true);});}
            else QTimer::singleShot(0,this,[this]{if(m_player&&m_player->playbackState()==QMediaPlayer::StoppedState&&m_player->mediaStatus()!=QMediaPlayer::EndOfMedia&&!m_finishing)stopSink();});
        });
    }
    updateRouting();emit stateChanged();
}
void SoundStudio::updateRouting(){
    const bool route=m_enabled&&m_music&&m_player&&m_visualizer&&m_originalOutput&&m_failure.isEmpty()&&!m_player->hasVideo()&&qFuzzyCompare(m_player->playbackRate(),1.);
    if(route==m_routing)return;
    stopSink(false);m_routing=route;
    m_internalMute=true;
    if(m_routing){
        // Keep the native renderer attached as the decoder's playback clock.
        // Only the processed worker output is audible.
        m_originalOutput->setMuted(true);m_visualizer->setCaptureAudio(true);connectCapture();m_watch.start();
    } else {
        m_watch.stop();if(m_originalOutput)m_originalOutput->setMuted(m_userMuted);
        if(m_visualizer)m_visualizer->setCaptureAudio(false);
    }
    m_internalMute=false;syncVolume();
    emit stateChanged();
}
void SoundStudio::stopSink(bool reconnect){
    m_budget->accepting.store(false);disconnect(m_captureConnection);
    // A direct decoder callback can already be running when disconnected.
    // Wait for its short admission section before resetting/deleting the worker.
    {std::lock_guard<std::mutex> lock(m_budget->ingress);}
    const auto token=++m_token;
    QMetaObject::invokeMethod(m_worker,[this,token]{m_worker->reset(token);},Qt::BlockingQueuedConnection);
    m_budget=std::make_shared<StudioBudget>();m_budget->lastFeedMs.store(studioClockMs());m_finishing=false;m_reduction=0;emit meterChanged();
    if(reconnect&&m_routing)connectCapture();
}
void SoundStudio::fail(const QString &reason){const bool finishing=m_finishing;m_failure=reason;updateRouting();emit stateChanged();if(finishing)emit playbackFinished();}
void SoundStudio::retry(){m_failure.clear();updateRouting();emit stateChanged();}
void SoundStudio::syncVolume(){
    const float volume=m_originalOutput&&!m_userMuted?m_originalOutput->volume():0;
    QMetaObject::invokeMethod(m_worker,[this,volume]{m_worker->volume(volume);});
}
void SoundStudio::prepareSeek(qint64 position){
    if(!m_routing)return;
    stopSink(false);m_budget->seekTarget.store(std::max<qint64>(0,position));
    m_budget->seekStartedMs.store(studioClockMs());connectCapture();
}
void SoundStudio::connectCapture(){
    if(!m_routing||!m_visualizer||!m_originalOutput||!m_player)return;
    const auto budget=m_budget;const auto token=m_token;auto *worker=m_worker;
    const auto device=m_originalOutput->device().isNull()?QMediaDevices::defaultAudioOutput():m_originalOutput->device();
    budget->paused.store(m_player->playbackState()!=QMediaPlayer::PlayingState);
    budget->lastFeedMs.store(studioClockMs());budget->accepting.store(true);
    m_captureConnection=connect(m_visualizer,&AudioVisualizer::decodedBuffer,worker,
        [budget,token,device,worker](const QAudioBuffer &buffer){
        std::lock_guard<std::mutex> lock(budget->ingress);
        if(!budget->accepting.load()||budget->paused.load()||!buffer.isValid()||!buffer.frameCount())return;
        auto reject=[&](const QString &reason){
            if(!budget->accepting.exchange(false))return;
            QMetaObject::invokeMethod(worker,[worker,token,reason]{worker->reject(token,reason);});
        };
        const auto f=buffer.format();const int rate=f.sampleRate(),channels=f.channelCount();
        if(rate<8000||rate>192000||channels<1||channels>2||f.bytesPerSample()<1||buffer.duration()>500000){reject(tr("Unsupported audio format"));return;}
        const auto target=budget->seekTarget.load();
        if(target>=0){
            if(buffer.startTime()<0){reject(tr("Studio cannot seek accurately in this file"));return;}
            if(buffer.startTime()+buffer.duration()<(target-20)*1000||buffer.startTime()>(target+250)*1000){
                if(studioClockMs()-budget->seekStartedMs.load()>1500)reject(tr("Studio could not synchronise the seek"));return;
            }
            budget->seekTarget.store(-1);
        }
        budget->lastFeedMs.store(studioClockMs());
        const auto duration=buffer.duration();
        // Reserve before posting so even a stalled audio worker cannot create
        // an unbounded Qt event queue. This limit is unchanged from 0.10.1.
        if(budget->queuedUs.fetch_add(duration)+duration>2000000){
            budget->queuedUs.fetch_sub(duration);reject(tr("Audio buffering failed; retry Studio"));return;
        }
        QMetaObject::invokeMethod(worker,[worker,buffer,device,budget,token]{worker->enqueue(buffer,device,budget,token);});
    },Qt::DirectConnection);
}
void SoundStudio::finish(){
    if(!m_routing){emit playbackFinished();return;}
    m_finishing=true;const auto token=m_token;QMetaObject::invokeMethod(m_worker,[this,token]{m_worker->finish(token);});
}
