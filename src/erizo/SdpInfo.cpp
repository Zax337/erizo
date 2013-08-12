/*
 * SDPProcessor.cpp
 */

#include <sstream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>

#include "rtputils.h"
#include "SdpInfo.h"

using std::endl;
namespace erizo {

    static const char *cand = "a=candidate:";
    static const char *crypto = "a=crypto:";
    static const char *group = "a=group:";
    static const char *video = "m=video";
    static const char *audio = "m=audio";
    static const char *ice_user = "a=ice-ufrag";
    static const char *ice_pass = "a=ice-pwd";
    static const char *ssrctag = "a=ssrc";
    static const char *savpf = "SAVPF";
    static const char *rtpmap = "a=rtpmap:";
    static const char *rtcpmux = "a=rtcp-mux";
    static const char *fp = "a=fingerprint";

    SdpInfo::SdpInfo() {
        isBundle = false;
        isRtcpMux = false;
        isFingerprint = false;
        hasAudio = false;
        hasVideo = false;
        profile = AVPF;
        audioSsrc = 0;
        videoSsrc = 0;

        printf("Generating internal RtpMap\n");

        RtpMap vp8;
        vp8.payloadType = VP8_90000_PT;
        vp8.encodingName = "VP8";
        vp8.clockRate = 90000;
        vp8.channels = 1;
        vp8.mediaType = VIDEO_TYPE;
        internalPayloadVector_.push_back(vp8);

        RtpMap ulpfec;
        ulpfec.payloadType = ULP_90000_PT;
        ulpfec.encodingName = "ulpfec";
        ulpfec.clockRate = 90000;
        ulpfec.channels = 1;
        ulpfec.mediaType = VIDEO_TYPE;
        internalPayloadVector_.push_back(ulpfec);

        RtpMap red;
        red.payloadType = RED_90000_PT;
        red.encodingName = "red";
        red.clockRate = 90000;
        red.channels = 1;
        red.mediaType = VIDEO_TYPE;
        internalPayloadVector_.push_back(red);

        RtpMap opus;
        opus.payloadType = OPUS_48000_PT;
        opus.encodingName = "opus";
        opus.clockRate = 48000;
        opus.channels = 2;
        opus.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(opus);
        //
        RtpMap isac16;
        isac16.payloadType = ISAC_16000_PT;
        isac16.encodingName = "ISAC";
        isac16.clockRate = 16000;
        isac16.channels = 1;
        isac16.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(isac16);

        RtpMap isac32;
        isac32.payloadType = ISAC_32000_PT;
        isac32.encodingName = "ISAC";
        isac32.clockRate = 32000;
        isac32.channels = 1;
        isac32.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(isac32);

        RtpMap pcmu;
        pcmu.payloadType = PCMU_8000_PT;
        pcmu.encodingName = "PCMU";
        pcmu.clockRate = 8000;
        pcmu.channels = 1;
        pcmu.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(pcmu);

        RtpMap pcma;
        pcma.payloadType = PCMA_8000_PT;
        pcma.encodingName = "PCMA";
        pcma.clockRate = 8000;
        pcma.channels = 1;
        pcma.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(pcma);

        RtpMap cn8;
        cn8.payloadType = CN_8000_PT;
        cn8.encodingName = "CN";
        cn8.clockRate = 8000;
        cn8.channels = 1;
        cn8.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(cn8);

        RtpMap cn16;
        cn16.payloadType = CN_16000_PT;
        cn16.encodingName = "CN";
        cn16.clockRate = 16000;
        cn16.channels = 1;
        cn16.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(cn16);

        RtpMap cn32;
        cn32.payloadType = CN_32000_PT;
        cn32.encodingName = "CN";
        cn32.clockRate = 32000;
        cn32.channels = 1;
        cn32.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(cn32);

        RtpMap cn48;
        cn48.payloadType = CN_48000_PT;
        cn48.encodingName = "CN";
        cn48.clockRate = 48000;
        cn48.channels = 1;
        cn48.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(cn48);

        RtpMap telephoneevent;
        telephoneevent.payloadType = TEL_8000_PT;
        telephoneevent.encodingName = "telephone-event";
        telephoneevent.clockRate = 8000;
        telephoneevent.channels = 1;
        telephoneevent.mediaType = AUDIO_TYPE;
        internalPayloadVector_.push_back(telephoneevent);

    }

    SdpInfo::~SdpInfo() {
    }

    bool SdpInfo::initWithSdp(const std::string& sdp) {
        processSdp(sdp);
        return true;
    }
    void SdpInfo::addCandidate(const CandidateInfo& info) {
        candidateVector_.push_back(info);

    }

    std::string SdpInfo::getWithPayload(const std::string& desc, int type) {
        std::string ret = desc; 
        std::ostringstream tmp;
        enum MediaType  mediaType = type < 0 ? VIDEO_TYPE : AUDIO_TYPE;
        tmp << "RTP/" << (profile == SAVPF ? "SAVPF " : "AVPF ");
        for (unsigned int it =0; it < payloadVector_.size(); it++){
            const RtpMap& payload_info = payloadVector_[it];
            if (payload_info.mediaType == mediaType)
                tmp << payload_info.payloadType << " ";
        }
        int pos = ret.find("ICE/SDP");
        if( pos != std::string::npos ) {
            ret.replace(pos, 7, tmp.str());
        } else {
            printf("There is something wrong with nice description.\n");
        }
        return ret;
    }

    void SdpInfo::addDesc(std::string desc, int type ) {
        int p = -1;
        switch(type) {
            case -1 :
                videoDesc_.assign(getWithPayload(desc, type));
                break;
            case  0 :
                videoDesc_.assign(getWithPayload(desc, -1));
                p = desc.find("video");
                desc.replace(p, 5, "audio");
                audioDesc_.assign(getWithPayload(desc, 1));
                break;
            case  1 :
                audioDesc_.assign(getWithPayload(desc, 1));
                break;
        }
    }

    void SdpInfo::addCrypto(const CryptoInfo& info) {
        cryptoVector_.push_back(info);
    }

    std::string SdpInfo::getSdp() {
        char* msidtemp = static_cast<char*>(malloc(10));
        gen_random(msidtemp,10);

        printf("Getting SDP\n");
        std::ostringstream sdp;
        sdp << "v=0\n" << "o=- 0 2 IN IP4 127.0.0.1\n" << "s=-\n" << "t=0 0\n";

        if (isBundle) {
            sdp << "a=group:BUNDLE audio video\n";
            sdp << "a=msid-semantic: WMS "<< msidtemp << endl;
        }
        //candidates audio
        //crypto audio
        if (!audioDesc_.empty()) {
            sdp << audioDesc_ ;
            if (isFingerprint) {
                sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
            }
            sdp << "a=sendrecv" << endl;
            sdp << "a=mid:audio" << endl;
            if (isRtcpMux)
                sdp << "a=rtcp-mux\n";
            for (unsigned int it = 0; it < cryptoVector_.size(); it++) {
                const CryptoInfo& cryp_info = cryptoVector_[it];
                if (cryp_info.mediaType == AUDIO_TYPE) {
                    sdp << "a=crypto:" << cryp_info.tag << " "
                        << cryp_info.cipherSuite << " " << "inline:"
                        << cryp_info.keyParams << endl;
                }
            }

            for (unsigned int it = 0; it < payloadVector_.size(); it++) {
                const RtpMap& rtp = payloadVector_[it];
                if (rtp.mediaType==AUDIO_TYPE)
                    sdp << "a=rtpmap:"<<rtp.payloadType << " " << rtp.encodingName << "/"
                        << rtp.clockRate <<"\n";
            }
            sdp << "a=maxptime:60" << endl;
            sdp << "a=ssrc:" << audioSsrc << " cname:o/i14u9pJrxRKAsu"        << endl <<
                   "a=ssrc:" << audioSsrc << " msid:"    << msidtemp << " a0" << endl <<
                   "a=ssrc:" << audioSsrc << " mslabel:" << msidtemp          << endl <<
                   "a=ssrc:" << audioSsrc << " label:"   << msidtemp << "a0"  <<endl;

        }

        //crypto audio
        if ( !videoDesc_.empty() ) {
            sdp << videoDesc_ ;
            sdp << "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset" << endl;
            sdp << "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time" << endl;
            if (isFingerprint) {
                sdp << "a=fingerprint:sha-256 "<< fingerprint << endl;
            }
            sdp << "a=sendrecv" << endl;
            sdp << "a=mid:video\n";
            if (isRtcpMux) 
                sdp << "a=rtcp-mux\n";
            sdp << "a=rtcp-fb:100 ccm fir" << endl;
            sdp << "a=rtcp-fb:100 nack" << endl;
            for (unsigned int it = 0; it < cryptoVector_.size(); it++) {
                const CryptoInfo& cryp_info = cryptoVector_[it];
                if (cryp_info.mediaType == VIDEO_TYPE) {
                    sdp << "a=crypto:" << cryp_info.tag << " "
                        << cryp_info.cipherSuite << " " << "inline:"
                        << cryp_info.keyParams << endl;
                }
            }

            for (unsigned int it = 0; it < payloadVector_.size(); it++) {
                const RtpMap& rtp = payloadVector_[it];
                if (rtp.mediaType==VIDEO_TYPE)
                    sdp << "a=rtpmap:" << rtp.payloadType << " " << rtp.encodingName << "/"
                        << rtp.clockRate <<"\n";
            }

            sdp << "a=ssrc:" << videoSsrc << " cname:o/i14u9pJrxRKAsu"        << endl <<
                   "a=ssrc:" << videoSsrc << " msid:"    << msidtemp << " v0" << endl<<
                   "a=ssrc:" << videoSsrc << " mslabel:" << msidtemp          << endl<<
                   "a=ssrc:" << videoSsrc << " label:"   << msidtemp << "v0"  << endl;
        }
        free (msidtemp);
        //   printf("sdp local \n %s\n",sdp.str().c_str());
        return sdp.str();
    }



    RtpMap *SdpInfo::getCodecByName(const std::string codecName, const unsigned int clockRate) {
        for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
            RtpMap& rtp = internalPayloadVector_[it];
            if (rtp.encodingName == codecName && rtp.clockRate == clockRate) {
                return &rtp;
            }
        }
        return NULL;
    }

    bool SdpInfo::supportCodecByName(const std::string codecName, const unsigned int clockRate) {
        RtpMap *rtp = getCodecByName(codecName, clockRate);
        if (rtp != NULL) {
            return supportPayloadType(rtp->payloadType);
        }
        return false;
    }

    bool SdpInfo::supportPayloadType(const int payloadType) {
        if (inOutPTMap.count(payloadType) > 0) {

            for (unsigned int it = 0; it < payloadVector_.size(); it++) {
                const RtpMap& rtp = payloadVector_[it];
                if (inOutPTMap[rtp.payloadType] == payloadType) {
                    return true;
                }
            }

        }
        return false;
    }

    bool SdpInfo::processSdp(const std::string& sdp) {

        std::string strLine;
        std::istringstream iss(sdp);
        char* line = (char*) malloc(1000);
        char** pieces = (char**) malloc(10000);
        char** cryptopiece = (char**) malloc(5000);

        MediaType mtype = OTHER;

        while (std::getline(iss, strLine)) {
            unsigned int pos = strLine.find_last_of('\r');
            if( pos == strLine.length() - 1) {
                strLine.erase(pos);
            }
            const char* theline = strLine.c_str();
            sprintf(line, "%s\n", theline);
            char* isVideo = strstr(line, video);
            char* isAudio = strstr(line, audio);
            char* isGroup = strstr(line, group);
            char* isCand = strstr(line, cand);
            char* isCrypt = strstr(line, crypto);
            char* isUser = strstr(line, ice_user);
            char* isPass = strstr(line, ice_pass);
            char* isSsrc = strstr(line, ssrctag);
            char* isSAVPF = strstr(line, savpf);
            char* isRtpmap = strstr(line,rtpmap);
            char* isRtcpMuxchar = strstr(line,rtcpmux);
            char* isFP = strstr(line,fp);

            if (isRtcpMuxchar){
                isRtcpMux = true;
            }
            if (isSAVPF){
                profile = SAVPF;
                printf("PROFILE %s (1 SAVPF)\n", isSAVPF);
            }
            if (isFP){
                char *pch;
                pch = strtok(line, ":");
                pch = strtok(NULL, " ");
                pch = strtok(NULL, " ");
                fingerprint = std::string(pch);
                isFingerprint = true;
                printf("Fingerprint %s \n", fingerprint.c_str());
            }
            if (isGroup) {
                isBundle = true;
            }
            if (isVideo) {
                mtype = VIDEO_TYPE;
                hasVideo = true;
            }
            if (isAudio) {
                mtype = AUDIO_TYPE;
                hasAudio = true;
            }
            if (isCand != NULL) {
                char *pch;
                pch = strtok(line, " :");
                pieces[0] = pch;
                int i = 0;
                while (pch != NULL) {
                    pch = strtok(NULL, " :");
                    pieces[i++] = pch;
                }

                processCandidate(pieces, i - 1, mtype);
            }
            if (isCrypt) {
                CryptoInfo crypinfo;
                char *pch;
                pch = strtok(line, " :");
                cryptopiece[0] = pch;
                int i = 0;
                while (pch != NULL) {
                    pch = strtok(NULL, " :");
                    cryptopiece[i++] = pch;
                }

                crypinfo.cipherSuite = std::string(cryptopiece[1]);
                crypinfo.keyParams = std::string(cryptopiece[3]);
                crypinfo.mediaType = mtype;
                cryptoVector_.push_back(crypinfo);
            }
            if (isUser) {
                char* pch;
                pch = strtok(line, " : \n");
                pch = strtok(NULL, " : \n");
                iceUsername_ = std::string(pch);

            }
            if (isPass) {
                char* pch;
                pch = strtok(line, " : \n");
                pch = strtok(NULL, ": \n");
                icePassword_ = std::string(pch);
            }
            if (isSsrc) {
                char* pch;
                pch = strtok(line, " : \n");
                pch = strtok(NULL, ": \n");
                if (mtype == VIDEO_TYPE) {
                    videoSsrc = strtoul(pch, NULL, 10);
                } else if (mtype == AUDIO_TYPE) {
                    audioSsrc = strtoul(pch, NULL, 10);
                }
            }
            // a=rtpmap:PT codec_name/clock_rate
            if(isRtpmap){
                RtpMap theMap;
                char* pch;
                pch = strtok(line, " : / \n");
                pch = strtok(NULL, " : / \n");
                unsigned int PT = strtoul(pch, NULL, 10);
                pch = strtok(NULL, " : / \n");
                std::string codecname(pch);
                pch = strtok(NULL, " : / \n");
                unsigned int clock = strtoul(pch, NULL, 10);
                theMap.payloadType = PT;
                theMap.encodingName = codecname;
                theMap.clockRate = clock;
                theMap.mediaType = mtype;

                bool found = false;
                for (unsigned int it = 0; it < internalPayloadVector_.size(); it++) {
                    const RtpMap& rtp = internalPayloadVector_[it];
                    if (rtp.encodingName == codecname && rtp.clockRate == clock) {
                        outInPTMap[PT] = rtp.payloadType;
                        inOutPTMap[rtp.payloadType] = PT;
                        found = true;
                        printf("Mapping %s/%d:%d to %s/%d:%d\n", codecname.c_str(), clock, PT, rtp.encodingName.c_str(), rtp.clockRate, rtp.payloadType);
                    }
                }
                if (found) {
                    payloadVector_.push_back(theMap);
                }
            }

        }

        free(line);
        free(pieces);
        free(cryptopiece);

        for (unsigned int i = 0; i < candidateVector_.size(); i++) {
            CandidateInfo& c = candidateVector_[i];
            c.username = iceUsername_;
            c.password = icePassword_;
            c.isBundle = isBundle;
        }

        return true;
    }

    std::vector<CandidateInfo>& SdpInfo::getCandidateInfos() {
        return candidateVector_;
    }

    std::vector<CryptoInfo>& SdpInfo::getCryptoInfos() {
        return cryptoVector_;
    }

    std::vector<RtpMap>& SdpInfo::getPayloadInfos(){
        return payloadVector_;
    }

    bool SdpInfo::processCandidate(char** pieces, int size, MediaType mediaType) {

        CandidateInfo cand;
        const char* types_str[10] = { "host", "srflx", "prflx", "relay" };
        cand.mediaType = mediaType;
        cand.foundation = pieces[0];
        cand.componentId = (unsigned int) strtoul(pieces[1], NULL, 10);

        cand.netProtocol = pieces[2];
        // libnice does not support tcp candidates, we ignore them
        if (cand.netProtocol.compare("UDP") && cand.netProtocol.compare("udp")) {
            return false;
        }
        //	a=candidate:0 1 udp 2130706432 138.4.4.143 52314 typ host  generation 0
        //		        0 1 2    3            4          5     6  7    8          9
        cand.priority = (unsigned int) strtoul(pieces[3], NULL, 10);
        cand.hostAddress = std::string(pieces[4]);
        cand.hostPort = (unsigned int) strtoul(pieces[5], NULL, 10);
        if (strcmp(pieces[6], "typ")) {
            return false;
        }
        unsigned int type = 1111;
        int p;
        for (p = 0; p < 4; p++) {
            if (!strcmp(pieces[7], types_str[p])) {
                type = p;
            }
        }
        switch (type) {
            case 0:
                cand.hostType = HOST;
                break;
            case 1:
                cand.hostType = SRLFX;
                break;
            case 2:
                cand.hostType = PRFLX;
                break;
            case 3:
                cand.hostType = RELAY;
                break;
            default:
                cand.hostType = HOST;
                break;
        }

        if (type == 3) {
            cand.relayAddress = std::string(pieces[8]);
            cand.relayPort = (unsigned int) strtoul(pieces[9], NULL, 10);
        }
        candidateVector_.push_back(cand);
        return true;
    }

    void SdpInfo::gen_random(char *s, const int len) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        for (int i = 0; i < len; ++i) {
            s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
        }

        s[len] = 0;
    }
}/* namespace erizo */

