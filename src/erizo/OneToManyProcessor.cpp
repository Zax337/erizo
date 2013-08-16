/*
 * OneToManyProcessor.cpp
 */

#include "OneToManyProcessor.h"
#include "WebRtcConnection.h"



namespace erizo {

    OneToManyProcessor::OneToManyProcessor() {

        sendVideoBuffer_ = (char*) malloc(20000);
        sendAudioBuffer_ = (char*) malloc(20000);
        publisher = NULL;
        feedbackSink_ = NULL;
        sentPackets_ = 0;
        srand(time(NULL));
        seqnb_ = rand() % 65535;
        timestamp_ = rand();

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

#if BISTRI_REC
        srand(time(NULL));
        std::stringstream filename;
        filename << "test" << rand() << ".avi";
        avformat_alloc_output_context2(&oc_, NULL, "avi", filename.str().c_str());
        AVStream * vstream = avformat_new_stream(oc_, ecodec_ctx->codec);
        stream_index = vstream->index;
        vstream->codec = ecodec_ctx;
        avio_open(&oc_->pb, filename.str().c_str(), AVIO_FLAG_WRITE);
        avformat_write_header(oc_, NULL);
#endif
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
        if(head->packettype == RTCP_Receiver_PT || head->packettype == RTCP_Feedback_PT) {
            printf("recibo feedback por donde no es %d\n", head->packettype);
            if ( feedbackSink_ ) {
                head->ssrc = htonl(publisher->getVideoSourceSSRC());
                feedbackSink_->deliverFeedback(buf,len);
            }
            return 0;
        }

        rtpheader* head2 = reinterpret_cast<rtpheader*>(buf);
        printf("Received\n");
        head2->print();
        size_t header = 12;//sizeof(rtpheader);
        if (head2->extension) {
            header += ntohs(head2->extensionlength) * 4 + 4; // RTP Extension header
        }
        size_t vp8hd  = 4; //sizeof(vp8desc); // dafuq ?
        size_t red    = 1; //sizeof(redhdr);
        size_t offset = header;

        //TODO VP8_PL does not mean anything there.. Must be first translate into inner representation
        if ((head2->payloadtype == RED_PL || head2->payloadtype == VP8_PL)) { // false -> disable mixing
            if( head2->payloadtype == RED_PL ) {
                redhdr * headred = reinterpret_cast<redhdr*>(buf + offset);
                headred->print();
                while( headred->p == 1 ) {
                    offset += red + 3;
                    headred = reinterpret_cast<redhdr*>(buf + offset);
                    headred->print();
                }
                if(headred->pl != VP8_PL ) {
                    printf("Quit because not vp8\n");
                    return 1;
                }
                ++offset;
            }
            vp8desc * vp8 = reinterpret_cast<vp8desc *>(buf + offset);
            vp8->print();
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
            /*
            if(cursor_ == 0) {
                timestamp_ = ntohl(head2->timestamp);
            }*/
            /*
               if( vp8->i ) { 
               do {
               offset++;
               } while( (buf[offset] >> 7) & 1);
               }*/
            memcpy(sendVideoBuffer_ + cursor_, buf + offset, len - offset);
            cursor_ += len - offset;
            if( head2->marker) {
                uint8_t * d = (uint8_t *) malloc(cursor_);
                memset(d, 0, cursor_);
                memcpy(d, sendVideoBuffer_, cursor_);
                printf("cursor %d\n", cursor_);
                SendVideoToSubscribers(d, cursor_);
                memset(sendVideoBuffer_, 0, cursor_);
                cursor_ = 0;
                return 0;
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
//                    printf("GOT FRAME\n");
                    av_init_packet(&packet);
                    packet.size = 0;
                    packet.data = NULL;
                    ret = avcodec_encode_video2(ecodec_ctx, &packet, frame, &got_output);
                    if (ret < 0) {
                        fprintf(stderr, "Error encoding frame\n");
                        return ret;
                    }
                    if (got_output) {
//                        printf("GOT OUTPUT %d\n", packet.size);
#if BISTRI_REC
                        packet.stream_index = stream_index;
                        av_write_frame(oc_, &packet);
#endif
                        SendVideoToSubscribers( packet.data, packet.size );
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
        }
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

    //TODO
    void OneToManyProcessor::SendVideoToSubscribers(uint8_t * data, int size ) {
        int nb_packet = size / (MAX_RTP_LEN - (12 + 1 + 1)) + 1;
        int payload_length = size / nb_packet; 
        assert(payload_length < MAX_RTP_LEN);
        uint8_t buf[MAX_RTP_LEN];
        memset(buf, 0, MAX_RTP_LEN);
        uint8_t header[HEADER_SIZE];
        memset(header, 0, HEADER_SIZE);
        int len = 0;
        int max_payload_len = MAX_RTP_LEN - HEADER_SIZE;

        header[0] = 0x80;
        header[1] = (VP8_PL & 0x7f);

        header[2] = (int) seqnb_ >> 8;
        header[3] = (uint8_t) seqnb_++;

        header[4] = timestamp_ >> 24;
        header[5] = (uint8_t)(timestamp_ >> 16);
        header[6] = (uint8_t)(timestamp_ >> 8 );
        header[7] = (uint8_t) timestamp_;   
        timestamp_+= 250;

        header[8]  = publisher->getVideoSourceSSRC() >> 24;
        header[9]  = (uint8_t)(publisher->getVideoSourceSSRC() >> 16);
        header[10] = (uint8_t)(publisher->getVideoSourceSSRC() >> 8 );
        header[11] = (uint8_t) publisher->getVideoSourceSSRC(); 
        printf("Ssrc %u\n", publisher->getVideoSourceSSRC());

        header[12] = 0x90;
        header[13] = 0x80;
        header[14] = frame_count_++ & 0x7f;
        memcpy(buf, header, HEADER_SIZE);
        rtpheader * rtp = reinterpret_cast<rtpheader *>(header); 
        vp8desc * vp8 = reinterpret_cast<vp8desc *>(header + 12);
        vp8desc_x * vp8_x = reinterpret_cast<vp8desc_x *>(header + 13);
        printf("Mixer\n");
        rtp->print();
        vp8->print();
        vp8_x->print();
        while(size > 0) {
            len = std::min(size, max_payload_len);
            if( len == size ) {
                buf[1] |= (0x01 << 7);
            }
            memcpy(buf + HEADER_SIZE, data, len);
            std::map<std::string, MediaSink *>::iterator it;
            for (it = subscribers.begin(); it != subscribers.end(); it++) {
                (*it).second->deliverVideoData((char *)buf, len + HEADER_SIZE);
            }
            sentPackets_++;
            data += len;
            size -= len;
            buf[0] &= ~0x10;
        }
    }


/*
    void OneToManyProcessor::FillRTPHeader(uint8_t * buf, bool marker) {
        rtpheader rtp = {0, 0, 0, 2, VP8_PL, marker ? 1 : 0, htons(seqnb_++ % 65535), htonl(timestamp_), htonl(publisher->localVideoSsrc_)};
        memcpy(buf, &rtp, sizeof(rtpheader));
    }

    void OneToManyProcessor::FillREDHeader(uint8_t * buf) {
        redhdr red = {VP8_PL,0};
        memcpy(buf, &red, sizeof(redhdr));
    }

    void OneToManyProcessor::FillVP8Header(uint8_t * buf, bool last, bool first) {
        vp8desc vp8_desc = {0, 0, 0, first, 0};
        memcpy(buf, &vp8_desc, sizeof(vp8_desc));
    }

    void OneToManyProcessor::FillPayload(uint8_t * buf, uint8_t * data, int size) {
        memcpy(buf, data, size);
    }
*/

    }/* namespace erizo */

