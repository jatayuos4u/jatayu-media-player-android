#include "PlaybackDecks.h"
#include <algorithm>
#include <cmath>
PlaybackDecks::PlaybackDecks(QObject *parent,MediaSource::DescriptorOpener opener,const QString &file):QObject(parent){
    m_decks[0]=std::make_unique<Deck>(opener,file,true);m_decks[1]=std::make_unique<Deck>(opener,file,false);
    for(int i=0;i<2;++i){
        auto &d=*m_decks[i];
        connect(&d.studio,&SoundStudio::settingsChanged,this,[this,i]{if(m_mirroring)return;m_mirroring=true;m_decks[1-i]->studio.copyConfigurationFrom(m_decks[i]->studio);m_mirroring=false;});
        connect(&d.player,&QMediaPlayer::playbackStateChanged,this,[this,i](QMediaPlayer::PlaybackState state){if(i==m_active&&state!=QMediaPlayer::PlayingState&&!m_mutating)cancelTransition();updateTimer();});
        connect(&d.player,&QMediaPlayer::playbackRateChanged,this,[this,i]{if(i==m_active&&player()->playbackRate()!=1.)cancelTransition();});
        connect(&d.player,&QMediaPlayer::errorOccurred,this,[this,i]{if(i!=m_active){m_starting=false;m_decks[i]->player.stop();}});
    }
    m_decks[1]->audio.setVolume(0);
    m_timer.setInterval(20);connect(&m_timer,&QTimer::timeout,this,&PlaybackDecks::tick);
}
bool PlaybackDecks::open(QObject *object,const QUrl &url){
    cancelTransition();if(object!=player())return false;
    const bool ok=m_decks[m_active]->source.open(player(),url);m_error=m_decks[m_active]->source.error();emit errorChanged();return ok;
}
void PlaybackDecks::setVideoOutput(QObject *output){m_video=output;player()->setVideoOutput(output);}
void PlaybackDecks::setVolume(double v){if(!std::isfinite(v))return;m_volume=std::clamp(v,0.,1.);if(m_fading)gains(m_inGain);else{m_decks[m_active]->audio.setVolume(m_volume);m_decks[1-m_active]->audio.setVolume(0);}}
void PlaybackDecks::setCrossfade(int seconds){m_seconds=std::clamp(seconds,0,8);if(!m_seconds)cancelTransition();updateTimer();}
void PlaybackDecks::stageNext(const QUrl &url){
    if(m_fading||m_staged==url)return;cancelTransition();m_staged=url;
    if(url.isEmpty()||!m_seconds)return;
    auto &next=*m_decks[1-m_active];next.audio.setVolume(0);next.studio.setMusic(true);next.player.setPlaybackRate(1.);
    next.source.open(&next.player,url);updateTimer();
}
void PlaybackDecks::cancelTransition(){
    if(m_mutating)return;m_mutating=true;
    auto &idle=*m_decks[1-m_active];idle.player.stop();idle.player.setSource({});idle.visualizer.setEnabled(false);
    m_staged=QUrl();m_starting=false;const bool was=m_fading;m_fading=false;m_outGain=0;m_inGain=1;
    idle.audio.setVolume(0);m_decks[m_active]->audio.setVolume(m_volume);m_mutating=false;updateTimer();if(was)emit fadingChanged();
}
void PlaybackDecks::gains(double in){m_inGain=std::clamp(in,0.,1.);m_outGain=1-m_inGain;m_decks[m_active]->audio.setVolume(m_volume*m_inGain);m_decks[1-m_active]->audio.setVolume(m_volume*m_outGain);}
void PlaybackDecks::handoff(){
    const int old=m_active;auto &out=m_decks[old]->player;auto &in=m_decks[1-old]->player;
    const auto oldUrl=out.source();const auto pos=out.position(),duration=out.duration();
    m_fadeDuration=std::min(m_seconds*1000,int(duration-pos));if(m_fadeDuration<300){cancelTransition();return;}
    m_mutating=true;m_active=1-old;m_starting=false;m_fading=true;m_clock.start();out.setVideoOutput(nullptr);in.setVideoOutput(m_video);m_decks[old]->visualizer.setEnabled(false);gains(0);
    emit fadingChanged();emit activeChanged();emit crossfadeStarted(oldUrl,in.source(),pos,duration);m_mutating=false;
}
void PlaybackDecks::updateTimer(){if(m_seconds&&!m_staged.isEmpty()&&player()->playbackState()==QMediaPlayer::PlayingState)m_timer.start();else m_timer.stop();}
void PlaybackDecks::tick(){
    if(m_fading){if(player()->playbackState()!=QMediaPlayer::PlayingState){cancelTransition();return;}const double t=double(m_clock.elapsed())/m_fadeDuration;gains(t);if(t>=1)cancelTransition();return;}
    if(!m_seconds||m_staged.isEmpty()||player()->playbackState()!=QMediaPlayer::PlayingState||player()->hasVideo()||player()->playbackRate()!=1.||player()->duration()<(m_seconds+2)*1000)return;
    auto &next=m_decks[1-m_active]->player;
    if(next.error()!=QMediaPlayer::NoError||next.hasVideo()||!next.hasAudio()||next.duration()<(m_seconds+1)*1000)return;
    const auto left=player()->duration()-player()->position();
    if(left<=m_seconds*1000+250&&left>500){
        if(!m_starting){m_starting=true;next.play();}
        // Allow the Studio sink's initial queue to prime at zero gain. Both
        // source clocks then run continuously throughout the overlap.
        if(next.position()>=200&&next.playbackState()==QMediaPlayer::PlayingState)handoff();
    }
}
