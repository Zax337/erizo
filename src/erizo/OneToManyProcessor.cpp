/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"
#include "WebRtcConnection.h"


namespace erizo {
    AVCodecContext * codec_ctx;
    AVCodec * codec;
    AVFrame * frame;
    AVCodecContext * ecodec_ctx;
    AVCodec * ecodec;
//    AVFormatContext * oc_;
//    int stream_index;


    OneToManyProcessor::OneToManyProcessor() {

        sendVideoBuffer_ = (char*) malloc(20000);
        sendAudioBuffer_ = (char*) malloc(20000);
        publisher = NULL;
        feedbackSink_ = NULL;
        sentPackets_ = 0;
        srand(time(NULL));
        seqnb_ = rand() % 65535;

        av_register_all();
      avformat_network_init();
      codec = avcodec_find_decoder(AV_CODEC_ID_VP8);
      codec_ctx = avcodec_alloc_context3(codec);
      avcodec_open2(codec_ctx, codec, NULL);
      frame = avcodec_alloc_frame();

      ecodec = avcodec_find_encoder(AV_CODEC_ID_VP8);
      ecodec_ctx = avcodec_alloc_context3(ecodec);
      ecodec_ctx->pix_fmt = PIX_FMT_YUV420P;
      ecodec_ctx->width  = 320;
      ecodec_ctx->height = 240;
      ecodec_ctx->qmin = 3;
      ecodec_ctx->time_base = (AVRational){1,30};
      avcodec_open2(ecodec_ctx, ecodec, NULL);
      cursor_ = 0;

/*
      srand(time(NULL));
      std::stringstream filename;
      filename << "test" << rand() << ".avi"; */
//      avformat_alloc_output_context2(&oc_, NULL, "rtp"/*"avi"*/, /*filename.str().c_str()*/"");
/*      AVStream * vstream = avformat_new_stream(oc_, ecodec_ctx->codec);
      stream_index = vstream->index;
      vstream->codec = ecodec_ctx;
//      avio_open(&oc_->pb, filename.str().c_str(), AVIO_FLAG_WRITE);
//      avformat_write_header(oc_, NULL);*/
    }

    OneToManyProcessor::~OneToManyProcessor() {
        this->closeAll();
        if (sendVideoBuffer_)
            delete sendVideoBuffer_;
        if (sendAudioBuffer_)
            delete sendAudioBuffer_;
    }

    int OneToManyProcessor::deliverAudioData(char* buf, int len) {
        if (subscribers.empty() || len <= 0)
            return 0;

        std::map<std::string, MediaSink*>::iterator it;
        for (it = subscribers.begin(); it != subscribers.end(); it++) {
            (*it).second->deliverAudioData(buf, len);
        }

        return 0;
    }

    int OneToManyProcessor::deliverVideoData(char* buf, int len) {
        if (subscribers.empty() || len <= 0)
            return 0;

        rtcpheader* head = reinterpret_cast<rtcpheader*>(buf);
        if(head->packettype == RTCP_Receiver_PT || head->packettype == RTCP_Feedback_PT){
            printf("recibo feedback por donde no es %d\n", head->packettype);
            if ( feedbackSink_ ) {
                head->ssrc = htonl(publisher->getVideoSourceSSRC());
                feedbackSink_->deliverFeedback(buf,len);
            }
            return 0;
        }
    size_t header = sizeof(rtpheader);
    size_t vp8hd  = 4;//sizeof(vp8desc); // dafuq ?
    size_t red    = sizeof(redhdr);
    size_t offset = header;

    rtpheader* head2 = reinterpret_cast<rtpheader*>(buf);
    if ((head2->payloadtype == RED_PL || head2->payloadtype == VP8_PL) && false) { // false -> disable mixing
        if( head2->payloadtype == RED_PL ) {
            bool pouet = false; 
            redhdr * headred = reinterpret_cast<redhdr*>(buf + offset);
            while( headred->p == 1 ) {
                printf("headred : %u %u\n", headred->pl, headred->p << 7 | headred->pl);
                if( headred->pl != VP8_PL ) pouet = true;
                offset += red + 3;
                headred = reinterpret_cast<redhdr*>(buf + offset);
            }
            if(headred->pl != VP8_PL || pouet ) {
                printf("Quit because not vp8\n");
                return 1;
            }
            ++offset;
        } else {
            printf("Non RED packet\n");
        }
        vp8desc * vp8 = reinterpret_cast<vp8desc *>(buf + offset);
        offset += vp8hd;
        if( vp8->x ) {
            ++offset;
            vp8desc_x * vp8_x = reinterpret_cast<vp8desc_x *>(buf + offset);
            if( vp8_x->i )
                ++offset;
            if( vp8_x->l )
                ++offset;
            if( vp8_x->t || vp8_x->k )
                ++offset;
        }
        if(cursor_ == 0) {
            if(timestamp_ == ntohl(head2->timestamp)) printf("Same timestamp %u\n", timestamp_);
            printf("timestamp %u\n", timestamp_);
            timestamp_ = ntohl(head2->timestamp);
        }
        /*
        if( vp8->i ) { 
            do {
                offset++;
            } while( (buf[offset] >> 7) & 1);
        }*/
        memcpy(sendVideoBuffer_ + cursor_, buf + offset, len - offset);
        cursor_ += len - offset;
        if( head2->marker) {
            int got_frame, got_output;
            AVPacket packet;
            av_init_packet(&packet);
            packet.size = cursor_;
            packet.data = (uint8_t *) sendVideoBuffer_;

            int ret = avcodec_decode_video2(codec_ctx, frame, &got_frame, &packet);
            memset(sendVideoBuffer_, 0, cursor_);
            cursor_ = 0;
            av_free_packet(&packet);
            if (ret < 0) {
                return ret;
            }
            if (got_frame) {
                printf("GOT FRAME\n");
                av_init_packet(&packet);
                packet.size = 0;
                packet.data = NULL;
                ret = avcodec_encode_video2(ecodec_ctx, &packet, frame, &got_output);
                if (ret < 0) {
                    fprintf(stderr, "Error encoding frame\n");
                    return ret;
                }
                if (got_output) {
                    printf("GOT OUTPUT %d\n", packet.size);
                    /*
                    packet.stream_index = stream_index;
                    av_write_frame(oc_, &packet);
                    */
                    SendVideoToSubscribers( &packet );
                }
                av_free_packet(&packet);
            }
        }
    } else {
        std::map<std::string, MediaSink*>::iterator it;
        for (it = subscribers.begin(); it != subscribers.end(); it++) {
            (*it).second->deliverVideoData(buf, len);
        }
        sentPackets_++;
        return 0;
    }

    void OneToManyProcessor::setPublisher(MediaSource* webRtcConn) {
        printf("SET PUBLISHER\n");
        publisher = webRtcConn;
        webRtcConn->muxer_ = this;
        feedbackSink_ = publisher->getFeedbackSink();
    }

    int OneToManyProcessor::deliverFeedback(char* buf, int len){
        feedbackSink_->deliverFeedback(buf,len);
        return 0;

    }

    void OneToManyProcessor::updateSsrcs() {
        for(std::map<std::string, MediaSink*>::iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
            (*it).second->setAudioSinkSSRC(publisher->getAudioSourceSSRC());
            (*it).second->setVideoSinkSSRC(publisher->getVideoSourceSSRC());
        }
    }

    void OneToManyProcessor::addSubscriber(MediaSink* webRtcConn, const std::string& peerId) {
        printf("Adding subscriber\n");
        printf("From %u, %u \n", publisher->getAudioSourceSSRC() , publisher->getVideoSourceSSRC());
        webRtcConn->setAudioSinkSSRC(publisher->getAudioSourceSSRC());
        webRtcConn->setVideoSinkSSRC(publisher->getVideoSourceSSRC());
        printf("Subscribers ssrcs: Audio %u, video, %u from %u, %u \n", webRtcConn->getAudioSinkSSRC(), webRtcConn->getVideoSinkSSRC(), this->publisher->getAudioSourceSSRC() , publisher->getVideoSourceSSRC());
        FeedbackSource* fbsource = webRtcConn->getFeedbackSource();

        if (fbsource!=NULL){
            printf("adding fbsource************************************************\n\n\n");
            fbsource->setFeedbackSink(this);
        }
        subscribers[peerId] = webRtcConn;
    }

    void OneToManyProcessor::removeSubscriber(const std::string& peerId) {
        if (this->subscribers.find(peerId) != subscribers.end()) {
            this->subscribers[peerId]->closeSink();
            this->subscribers.erase(peerId);
        }
    }

    void OneToManyProcessor::closeSink(){
        this->close();
    }

    void OneToManyProcessor::close(){
        this->closeAll();
    }

    void OneToManyProcessor::closeAll() {
        std::map<std::string, MediaSink*>::iterator it;
        for (it = subscribers.begin(); it != subscribers.end(); it++) {
            (*it).second->closeSink();
        }
        this->publisher->closeSource();
    }
    return 0;
  }


//TODO
  //void OneToManyProcessor::SendVideoToSubscribers(uint8_t * data, int size ) {
  void OneToManyProcessor::SendVideoToSubscribers(AVPacket * packet ) {
        std::map<std::string, MediaSink *>::iterator it;
        //uint8_t * buf = (uint8_t *) malloc(sizeof(uint8_t) * size);
        for (it = subscribers.begin(); it != subscribers.end(); it++) {
//            memset(buf, 0, size);
//            memcpy(buf, data, size);
//            (*it).second->receiveVideoData(buf, size, true);
            (*it).second->receiveVideoData(packet);
        }
        //free(buf);
            /*
      int nb_packet = size / (MAX_RTP_LEN - (sizeof(rtpheader) + sizeof(redhdr) + sizeof(vp8desc))) + 1;
      int payload_length = size / nb_packet; 
      assert(payload_length < MAX_RTP_LEN);
      int offset = 0;
      uint8_t buf[MAX_RTP_LEN];
      memset(buf, 0, MAX_RTP_LEN);
      bool marker = false;
      int len = payload_length;
      int header = 0;
      while ( offset < size ) {
          if ( offset + payload_length > size ) {
              marker = true;
              len = size - offset;
          }
          assert(len + 13 < MAX_RTP_LEN);

          FillRTPHeader(buf,  marker);
          header += sizeof(rtpheader);
          FillREDHeader(buf + header);
          header += sizeof(redhdr);
          FillVP8Header(buf + header, marker, offset == 0 );
          header++;
          FillPayload(buf + header, data + offset, len);

          std::map<std::string, WebRtcConnection*>::iterator it;
          for (it = subscribers.begin(); it != subscribers.end(); it++) {
              memset(sendVideoBuffer_, 0, MAX_RTP_LEN);
              memcpy(sendVideoBuffer_, buf, len + header);
              (*it).second->receiveVideoData(sendVideoBuffer_, header + len);
          }

          offset += payload_length;
          header = 0;
          memset(buf, 0, MAX_RTP_LEN);
      }
      */
  }

  void OneToManyProcessor::FillRTPHeader(uint8_t * buf, bool marker) {
//      struct timeval ts;
//      gettimeofday(&ts, NULL);
      rtpheader rtp = {0, 0, 0, 2, VP8_PL, marker ? 1 : 0, htons(seqnb_++ % 65535), htonl(timestamp_), htonl(publisher->localVideoSsrc_)};
      memcpy(buf, &rtp, sizeof(rtpheader));
  }

  void OneToManyProcessor::FillREDHeader(uint8_t * buf) {
  //    buf[sizeof(rtpheader)] = static_cast<uint8_t>(VP8_PL);
      redhdr red = {VP8_PL,0};
      memcpy(buf, &red, sizeof(redhdr));
  }

  void OneToManyProcessor::FillVP8Header(uint8_t * buf, bool last, bool first) {
      /*
      vp8desc vp8_desc = {first, 0x3, 0, 0,0};
      if(last) {
          vp8_desc.fi = 0x2;
      } else if (first) {
          vp8_desc.fi = 0x1;
      }*/
      vp8desc vp8_desc = {0, 0, 0, first, 0};
      memcpy(buf, &vp8_desc, sizeof(vp8_desc));
  }

  void OneToManyProcessor::FillPayload(uint8_t * buf, uint8_t * data, int size) {
      memcpy(buf, data, size);
  }


}/* namespace erizo */

