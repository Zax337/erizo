/*
 * WebRTCConnection.cpp
 */

#include <cstdio>

#include "WebRtcConnection.h"
#include "DtlsTransport.h"
#include "SdesTransport.h"

#include "SdpInfo.h"
#include "rtputils.h"

namespace erizo {

    WebRtcConnection::WebRtcConnection() {

        printf("WebRtcConnection constructor\n");
        video_ = 0;
        audio_ = 0;
        sequenceNumberFIR_ = 0;
        bundle_ = false;
        srand(time(NULL));
        int r = rand();
        this->setVideoSinkSSRC(r);
        srand(r);
        this->setAudioSinkSSRC(rand());
        videoSink_ = NULL;
        audioSink_ = NULL;
        fbSink_ = NULL;
        sourcefbSink_ = this;
        sinkfbSource_ = this;

        globalState_ = INITIAL;
        connStateListener_ = NULL;

        sending_ = true;
        send_Thread_ = boost::thread(&WebRtcConnection::sendLoop, this);

        videoTransport_ = NULL;
        audioTransport_ = NULL;

        deliverMediaBuffer_ = (char*)malloc(3000);
        muxer_ = NULL;

        printf("WebRtcConnection constructor end\n");
    }

    WebRtcConnection::~WebRtcConnection() {

        this->close();
        free(deliverMediaBuffer_);
    }

    bool WebRtcConnection::init() {
        return true;
    }

    void WebRtcConnection::close() {
        if (videoTransport_ != NULL) {
            videoTransport_->close();
        }
        if (audioTransport_ != NULL) {
            audioTransport_->close();
        }
        if (sending_ != false) {
            sending_ = false;
            send_Thread_.join();
        }
    }

    void WebRtcConnection::closeSink(){
        this->close();
    }

    void WebRtcConnection::closeSource(){
        this->close();
    }

    bool WebRtcConnection::setRemoteSdp(const std::string &sdp, const std::string& stunServ, const int stunPort) {
        printf("Set Remote SDP\n %s", sdp.c_str());
        remoteSdp_.initWithSdp(sdp);

        video_ = (remoteSdp_.videoSsrc == 0 ? false : true);
        audio_ = (remoteSdp_.audioSsrc == 0 ? false : true);

        CryptoInfo cryptLocal_video;
        CryptoInfo cryptLocal_audio;
        CryptoInfo cryptRemote_video;
        CryptoInfo cryptRemote_audio;

        bundle_ = remoteSdp_.isBundle;
        printf("Is bundle? %d %d \n", bundle_, true);
        std::vector<RtpMap> payloadRemote = remoteSdp_.getPayloadInfos();
        localSdp_.getPayloadInfos() = remoteSdp_.getPayloadInfos();
        localSdp_.isBundle = bundle_;
        localSdp_.isRtcpMux = remoteSdp_.isRtcpMux;
        localSdp_.profile = remoteSdp_.profile;

        printf("Video %d videossrc %u Audio %d audio ssrc %u Bundle %d \n", video_, remoteSdp_.videoSsrc, audio_, remoteSdp_.audioSsrc,  bundle_);

        printf("Setting SSRC to localSdp %u\n", this->getVideoSinkSSRC());
        localSdp_.videoSsrc = getVideoSinkSSRC();
        localSdp_.audioSsrc = getAudioSinkSSRC();

        setVideoSourceSSRC(remoteSdp_.videoSsrc);
        setAudioSourceSSRC(remoteSdp_.audioSsrc);

        if (remoteSdp_.profile == SAVPF) {
            if (remoteSdp_.isFingerprint) {
                // DTLS-SRTP
                if (remoteSdp_.hasVideo) {
                    videoTransport_ = new DtlsTransport(VIDEO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, stunServ, stunPort, this);
                }
                if (remoteSdp_.hasAudio) {
                    audioTransport_ = new DtlsTransport(AUDIO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, stunServ, stunPort, this);
                }
            } else {
                // SDES
                std::vector<CryptoInfo> crypto_remote = remoteSdp_.getCryptoInfos();
                for (unsigned int it = 0; it < crypto_remote.size(); it++) {
                    CryptoInfo cryptemp = crypto_remote[it];
                    if (cryptemp.mediaType == VIDEO_TYPE
                            && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
                        videoTransport_ = new SdesTransport(VIDEO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, stunServ, stunPort, &cryptemp, this);
                    } else if (!bundle_ && cryptemp.mediaType == AUDIO_TYPE
                            && !cryptemp.cipherSuite.compare("AES_CM_128_HMAC_SHA1_80")) {
                        audioTransport_ = new SdesTransport(AUDIO_TYPE, "", bundle_, remoteSdp_.isRtcpMux, stunServ, stunPort, &cryptemp, this);
                    }
                }
            }
        }

        if( muxer_ ) {
            muxer_->updateSsrcs();
        }
        return true;
    }

    std::string WebRtcConnection::getLocalSdp() {
        printf("Getting SDP\n");
        if (videoTransport_ != NULL) {
            localSdp_.addDesc(videoTransport_->nice_->getLocalDescription(), bundle_ ? 0 : -1);
            videoTransport_->processLocalSdp(&localSdp_);
        }
        printf("Video SDP done.\n");
        if (!bundle_ && audioTransport_ != NULL) {
            localSdp_.addDesc(audioTransport_->nice_->getLocalDescription(), 1);
            audioTransport_->processLocalSdp(&localSdp_);
        }
        printf("Audio SDP done.\n");
        localSdp_.profile = remoteSdp_.profile;
        return localSdp_.getSdp();
  }

  void WebRtcConnection::openFFMpegContext(std::string& ip, int port) {
      ecodec = avcodec_find_encoder(AV_CODEC_ID_VP8);
      ecodec_ctx = avcodec_alloc_context3(ecodec);
      ecodec_ctx->pix_fmt = PIX_FMT_YUV420P;
      ecodec_ctx->width  = 320;
      ecodec_ctx->height = 240;
      ecodec_ctx->qmin = 3;
      ecodec_ctx->time_base = (AVRational){1,30};
      avcodec_open2(ecodec_ctx, ecodec, NULL);

      srand(time(NULL));
      avformat_alloc_output_context2(&oc_, NULL, "rtp"/*"avi"*/, /*filename.str().c_str()*/ NULL);
      av_opt_set_int(oc_->priv_data, "payload_type", 100, 0);
      printf("ssrc %d \n", localVideoSsrc_);
      av_opt_set_int(oc_->priv_data, "ssrc", localVideoSsrc_, 0);
      AVStream * vstream = avformat_new_stream(oc_, ecodec_ctx->codec);
      vstream->codec = ecodec_ctx;
      std::stringstream url;
      url << "rtp://" << ip << ":" << port;
      int ret = avio_open(&oc_->pb, url.str().c_str(), AVIO_FLAG_WRITE);
      printf("ret %d %s\n", ret, url.str().c_str());
      assert(ret== 0);
      avformat_write_header(oc_, NULL);
  }


    int WebRtcConnection::deliverAudioData(char* buf, int len) {
        boost::mutex::scoped_lock lock(receiveAudioMutex_);
        writeSsrc(buf, len, this->getAudioSinkSSRC());
        rtpheader *head = reinterpret_cast<rtpheader*> (buf);
        if( remoteSdp_.inOutPTMap[head->payloadtype] != 0 ) {
            head->payloadtype = remoteSdp_.inOutPTMap[head->payloadtype];
        }
        if (bundle_){
            if (videoTransport_ != NULL) {
                videoTransport_->write(buf, len);
            }
        } else if (audioTransport_ != NULL) {
            audioTransport_->write(buf, len);
        }
        return len;
    }

    int WebRtcConnection::deliverVideoData(char* buf, int len) {
        boost::mutex::scoped_lock lock(receiveAudioMutex_);
        rtpheader *head = (rtpheader*) buf;
        if( remoteSdp_.inOutPTMap[head->payloadtype] != 0 ) {
            head->payloadtype = remoteSdp_.inOutPTMap[head->payloadtype];
        }

        if (head->payloadtype == RED_90000_PT) {
            int totalLength = 12;

            if (head->extension) {
                totalLength += ntohs(head->extensionlength) * 4 + 4; // RTP Extension header
            }
            int rtpHeaderLength = totalLength;
            redheader *redhead = (redheader*) (buf + totalLength);

            //redhead->payloadtype = remoteSdp_.inOutPTMap[redhead->payloadtype];
            if (!remoteSdp_.supportPayloadType(head->payloadtype)) {
                while (redhead->follow) {
                    totalLength += redhead->getLength() + 4; // RED header
                    redhead = (redheader*) (buf + totalLength);
                }
                // Parse RED packet to VP8 packet.
                // Copy RTP header
                memcpy(deliverMediaBuffer_, buf, rtpHeaderLength);
                // Copy payload data
                memcpy(deliverMediaBuffer_ + totalLength, buf + totalLength + 1, len - totalLength - 1);
                // Copy payload type
                rtpheader *mediahead = (rtpheader*) deliverMediaBuffer_;
                mediahead->payloadtype = remoteSdp_.inOutPTMap[redhead->payloadtype];
                buf = deliverMediaBuffer_;
                len = len - 1 - totalLength + rtpHeaderLength;
            }
        }
        writeSsrc(buf, len, this->getVideoSinkSSRC());
        if (videoTransport_ != NULL) {
            videoTransport_->write(buf, len);
        }
        return len;
    }

    int WebRtcConnection::deliverFeedback(char* buf, int len){
        // Check where to send the feedback
        rtcpheader *chead = (rtcpheader*) buf;
        //writeSsrc(buf, len, ntohl(chead->ssrcsource));
        if (ntohl(chead->ssrcsource) == this->getVideoSourceSSRC()) {
            chead->ssrc=htonl(this->getVideoSinkSSRC());
        } else {
            chead->ssrc=htonl(this->getAudioSinkSSRC());
        }

        if (bundle_){
            if (videoTransport_ != NULL) {
                videoTransport_->write(buf, len);
            }
        } else {
            // TODO: Check where to send the feedback
            if (videoTransport_ != NULL) {
                videoTransport_->write(buf, len);
            }
        }
        return len;
    }

    void WebRtcConnection::writeSsrc(char* buf, int len, int ssrc) {
        rtpheader *head = (rtpheader*) buf;
        rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);
        if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_Feedback_PT) {
            chead->ssrc=htonl(ssrc);
        } else {
            head->ssrc=htonl(ssrc);
        }
    }

    void WebRtcConnection::onTransportData(char* buf, int len, Transport *transport) {
        boost::mutex::scoped_lock lock(writeMutex_);
        if (audioSink_ == NULL && videoSink_ == NULL && fbSink_==NULL)
            return;
        int length = len;
        rtcpheader *chead = reinterpret_cast<rtcpheader*> (buf);

        if (chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_Feedback_PT){
            // Feedback!!
            if (fbSink_ != NULL) {
                fbSink_->deliverFeedback(buf,length);
            }
        } else {
            // RTP or RTCP Sender Report
            rtpheader * head = reinterpret_cast<rtpheader*> (buf);
            rtcpheader * chead = reinterpret_cast<rtcpheader*> (buf);
            if( remoteSdp_.outInPTMap[head->payloadtype] != 0 ) {
                head->payloadtype = remoteSdp_.outInPTMap[head->payloadtype];
            }
            if (bundle_) {

                // Check incoming SSRC
                unsigned int recvSSRC = ntohl(head->ssrc);

                if (chead->packettype == RTCP_Sender_PT) { //Sender Report
                    recvSSRC = ntohl(chead->ssrc);
                }

                // Deliver data
                if (recvSSRC == this->getVideoSourceSSRC() || recvSSRC == this->getVideoSinkSSRC()) {
                    videoSink_->deliverVideoData(buf, length);
                } else if (recvSSRC == this->getAudioSourceSSRC() || recvSSRC == this->getAudioSinkSSRC()) {
                    audioSink_->deliverAudioData(buf, length);
                } else {
                    printf("Unknown SSRC %u, localVideo %u, remoteVideo %u, ignoring\n", recvSSRC, this->getVideoSourceSSRC(), this->getVideoSinkSSRC());
                }
            } else if (transport->mediaType == AUDIO_TYPE) {
                if (audioSink_ != NULL) {
                    // Firefox does not send SSRC in SDP
                    if (this->getAudioSourceSSRC() == 0) {
                        printf("Audio Source SSRC is %d\n", ntohl(head->ssrc));
                        this->setAudioSourceSSRC(ntohl(head->ssrc));
                        this->updateState(TRANSPORT_READY, transport);
                    }
                    head->ssrc = htonl(this->getAudioSinkSSRC());
                    audioSink_->deliverAudioData(buf, length);
                }
            } else if (transport->mediaType == VIDEO_TYPE) {
                if (videoSink_ != NULL) {
                    // Firefox does not send SSRC in SDP
                    if (this->getVideoSourceSSRC() == 0) {
                        printf("Video Source SSRC is %d\n", ntohl(head->ssrc));
                        this->setVideoSourceSSRC(ntohl(head->ssrc));
                        this->updateState(TRANSPORT_READY, transport);
                    }

                    head->ssrc = htonl(this->getVideoSinkSSRC());
                    videoSink_->deliverVideoData(buf, length);
                }
            }
        }
    }

    int WebRtcConnection::sendFirPacket() {
        printf("SendingFIR\n");
        sequenceNumberFIR_++; // do not increase if repetition
        int pos = 0;
        uint8_t rtcpPacket[50];
        // add full intra request indicator
        uint8_t FMT = 4;
        rtcpPacket[pos++] = (uint8_t) 0x80 + FMT;
        rtcpPacket[pos++] = (uint8_t) 206;

        //Length of 4
        rtcpPacket[pos++] = (uint8_t) 0;
        rtcpPacket[pos++] = (uint8_t) (4);

        // Add our own SSRC
        uint32_t* ptr = reinterpret_cast<uint32_t*>(rtcpPacket + pos);
        ptr[0] = htonl(this->getVideoSinkSSRC());
        pos += 4;

        rtcpPacket[pos++] = (uint8_t) 0;
        rtcpPacket[pos++] = (uint8_t) 0;
        rtcpPacket[pos++] = (uint8_t) 0;
        rtcpPacket[pos++] = (uint8_t) 0;
        // Additional Feedback Control Information (FCI)
        uint32_t* ptr2 = reinterpret_cast<uint32_t*>(rtcpPacket + pos);
        ptr2[0] = htonl(this->getVideoSourceSSRC());
        pos += 4;

        rtcpPacket[pos++] = (uint8_t) (sequenceNumberFIR_);
        rtcpPacket[pos++] = (uint8_t) 0;
        rtcpPacket[pos++] = (uint8_t) 0;
        rtcpPacket[pos++] = (uint8_t) 0;

        if (videoTransport_ != NULL) {
            videoTransport_->write((char*)rtcpPacket, pos);
        }

        return pos;
    }

    void WebRtcConnection::setWebRTCConnectionStateListener(
            WebRtcConnectionStateListener* listener) {
        this->connStateListener_ = listener;
    }

    void WebRtcConnection::updateState(TransportState state, Transport * transport) {
        boost::mutex::scoped_lock lock(updateStateMutex_);
        WebRTCState temp = INITIAL;
        if (audioTransport_ == NULL && videoTransport_ == NULL) {
            return;
        }

        if (state == TRANSPORT_STARTED &&
                (!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_STARTED)) &&
                (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_STARTED))) {
            if (remoteSdp_.hasVideo) {
                videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
            }
            if (!bundle_ && remoteSdp_.hasAudio) {
                audioTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
            }
            temp = STARTED;
        }

        if (state == TRANSPORT_READY &&
                (!remoteSdp_.hasAudio || (audioTransport_ != NULL && audioTransport_->getTransportState() == TRANSPORT_READY)) &&
                (!remoteSdp_.hasVideo || (videoTransport_ != NULL && videoTransport_->getTransportState() == TRANSPORT_READY))) {
            if ((!remoteSdp_.hasAudio || this->getAudioSourceSSRC() != 0) &&
                    (!remoteSdp_.hasVideo || this->getVideoSourceSSRC() != 0)) {
                temp = READY;
            }


        }

        if (transport != NULL && transport == videoTransport_ && bundle_) {
            if (state == TRANSPORT_STARTED) {
                videoTransport_->setRemoteCandidates(remoteSdp_.getCandidateInfos());
                temp = STARTED;
            }
            if (state == TRANSPORT_READY) {
                temp = READY;
            }
        }

        if (temp == READY) {
            printf("Ready to send and receive media!!\n");
        }

        if (temp < 0) {
            return;
        }

        if (temp == globalState_ || (temp == STARTED && globalState_ == READY))
            return;

        globalState_ = temp;
        if (connStateListener_ != NULL)
            connStateListener_->connectionStateChanged(globalState_);
    }

    void WebRtcConnection::queueData(int comp, const char* buf, int length, Transport *transport) {
        receiveVideoMutex_.lock();
        if (sendQueue_.size() < 1000) {
            dataPacket p_;
            memset(p_.data, 0, length);
            memcpy(p_.data, buf, length);
            p_.comp = comp;
            if (transport->mediaType == VIDEO_TYPE) {
                p_.type = VIDEO_PACKET;
            } else {
                p_.type = AUDIO_PACKET;
            }

            p_.length = length;
            sendQueue_.push(p_);
        }
        receiveVideoMutex_.unlock();
    }

    WebRTCState WebRtcConnection::getCurrentState() {
        return globalState_;
    }

    void WebRtcConnection::sendLoop() {

        while (sending_ == true) {
            receiveVideoMutex_.lock();
            if (sendQueue_.size() > 0) {
                if (sendQueue_.front().type == AUDIO_PACKET && audioTransport_!=NULL) {
                    audioTransport_->writeOnNice(sendQueue_.front().comp, sendQueue_.front().data,
                            sendQueue_.front().length);
                } else {
                    videoTransport_->writeOnNice(sendQueue_.front().comp, sendQueue_.front().data,
                            sendQueue_.front().length);
                }
                sendQueue_.pop();
                receiveVideoMutex_.unlock();
            } else {
                receiveVideoMutex_.unlock();
                usleep(1000);
            }
        }
    }
}
/* namespace erizo */
