/*
 * DtlsConnection.cpp
 */
#include <iostream>
#include <cassert>

#include "DtlsTransport.h"
#include "NiceConnection.h"
#include "SrtpChannel.h"

#include "dtls/DtlsFactory.h"

#include "rtputils.h"

using namespace erizo;
using namespace std;
using namespace dtls;

DtlsTransport::DtlsTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, const std::string& stunServ, const int stunPort, const std::string& cred_id, const std::string& cred_pass, TransportListener *transportListener)
                            :Transport(med, transport_name, bundle, rtcp_mux, transportListener) {
    cout << "Initializing DtlsTransport" << endl;
    updateTransportState(TRANSPORT_INITIAL);

    readyRtp  = false;
    readyRtcp = false;

    dtlsRtp  = new DtlsSocketContext();
    dtlsRtcp = NULL;

    DtlsSocket *mSocket = (new DtlsFactory())->createClient(std::auto_ptr<DtlsSocketContext>(dtlsRtp));
    dtlsRtp->setSocket(mSocket);
    dtlsRtp->setDtlsReceiver(this);

    srtp_  = NULL;
    srtcp_ = NULL;
    protectBuf_   = reinterpret_cast<char*>(malloc(10000));
    unprotectBuf_ = reinterpret_cast<char*>(malloc(10000));
    int comps = 1;
    if (!rtcp_mux) {
      comps = 2;
      dtlsRtcp = new DtlsSocketContext();
      mSocket=(new DtlsFactory())->createClient(std::auto_ptr<DtlsSocketContext>(dtlsRtcp));
      dtlsRtcp->setSocket(mSocket);
      dtlsRtcp->setDtlsReceiver(this);
    }
    bundle_ = bundle;
    nice_ = new NiceConnection(med, transport_name, stunServ, stunPort, cred_id, cred_pass, comps);
    nice_->setNiceListener(this);
    nice_->start();
}

DtlsTransport::~DtlsTransport() {

  this->close();

  free(protectBuf_);
  free(unprotectBuf_);

}

void DtlsTransport::close() {
  if (dtlsRtp != NULL) {
      dtlsRtp->stop();
  }
  if (dtlsRtcp != NULL) {
      dtlsRtcp->stop();
  }
  if (srtp_ != NULL) {
    free(srtp_);
  }
  if (srtcp_ != NULL) {
    free(srtcp_);
  }
  if (nice_ != NULL) {
     nice_->close();
     nice_->join();
     delete nice_;
  }
}

void DtlsTransport::onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice) {
    boost::mutex::scoped_lock lock(readMutex_);
    int length = len;
    SrtpChannel *srtp = srtp_;

    if (DtlsTransport::isDtlsPacket(data, len)) {
      printf("Received DTLS message from %u\n", component_id);
      if (component_id == 1) {
        dtlsRtp->read(reinterpret_cast<unsigned char*>(data), len);
      } else {
        dtlsRtcp->read(reinterpret_cast<unsigned char*>(data), len);
      }

      return;
    } else if (this->getTransportState() == TRANSPORT_READY) {
      memset(unprotectBuf_, 0, len);
      memcpy(unprotectBuf_, data, len);

      if (dtlsRtcp != NULL && component_id == 2) {
        srtp = srtcp_;
      }

      if (srtp != NULL){
          rtcpheader *chead = reinterpret_cast<rtcpheader*> (unprotectBuf_);
          if (chead->packettype == RTCP_Sender_PT || 
              chead->packettype == RTCP_Receiver_PT || 
              chead->packettype == RTCP_Feedback_PT){
            if (chead->packettype == RTCP_Feedback_PT) {
              printf("NACK received\n");
            }

            if(srtp->unprotectRtcp(unprotectBuf_, &length)<0)
              return;
         } else {
            if(srtp->unprotectRtp(unprotectBuf_, &length)<0)
              return;
          }
      } else {
        return;
      }

      if (length <= 0)
          return;

      getTransportListener()->onTransportData(unprotectBuf_, length, this);
    }
}

void DtlsTransport::write(char* data, int len) {
   boost::mutex::scoped_lock lock(writeMutex_);
    int length = len;
    SrtpChannel *srtp = srtp_;

    int comp = 1;
    if (this->getTransportState() == TRANSPORT_READY) {
      memset(protectBuf_, 0, len);
      memcpy(protectBuf_, data, len);

      rtcpheader *chead = reinterpret_cast<rtcpheader*> (protectBuf_);
      if (chead->packettype == RTCP_Sender_PT || chead->packettype == RTCP_Receiver_PT || chead->packettype == RTCP_Feedback_PT) {
        if (!rtcp_mux_) {
          comp = 2;
        }
        if (dtlsRtcp != NULL) {
          srtp = srtcp_;
        }
        if (srtp && nice_->iceState == NICE_READY) {
          if(srtp->protectRtcp(protectBuf_, &length)<0) {
            return;
          }
        }
      }
      else{
        comp = 1;

        if (srtp && nice_->iceState == NICE_READY) {
          if(srtp->protectRtp(protectBuf_, &length)<0) {
            return;
          }
        }
      }
      if (length <= 10) {
        return;
      }
      if (nice_->iceState == NICE_READY) {
          getTransportListener()->queueData(comp, protectBuf_, length, this);
      }
    }
}

void DtlsTransport::writeDtls(DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) {
  int comp = 1;
  if (ctx == dtlsRtcp) {
    comp = 2;
  }
  printf("Sending DTLS message to %d\n", comp);
  nice_->sendData(comp, data, len);
}

void DtlsTransport::onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey,std::string serverKey, std::string srtp_profile) {
    if (ctx == dtlsRtp) {
      printf("Setting RTP srtp params\n");
      srtp_ = new SrtpChannel();
      srtp_->setRtpParams((char*) clientKey.c_str(), (char*) serverKey.c_str());
      readyRtp = true;
      if (dtlsRtcp == NULL) {
        readyRtcp = true;
      }
    }
    if (ctx == dtlsRtcp) {
      printf("Setting RTCP srtp params\n");
      srtcp_ = new SrtpChannel();
      srtcp_->setRtpParams((char*) clientKey.c_str(), (char*) serverKey.c_str());
      readyRtcp = true;
    }
    if (readyRtp && readyRtcp) {
      updateTransportState(TRANSPORT_READY);
    }

}

std::string DtlsTransport::getMyFingerprint() {
    return dtlsRtp->getFingerprint();
}

void DtlsTransport::updateIceState(IceState state, NiceConnection *conn) {
    cout << "New NICE state " << state << " " << mediaType << " " << bundle_ << endl;
    if (state == NICE_CANDIDATES_GATHERED) {
      updateTransportState(TRANSPORT_STARTED);
    }
    if (state == NICE_READY) {
        dtlsRtp->start();
        if (dtlsRtcp != NULL) {
          dtlsRtcp->start();
        }
    }
}

void DtlsTransport::processLocalSdp(SdpInfo *localSdp_) {
  cout << "Processing Local SDP in DTLS Transport" << endl;
  std::vector<CandidateInfo> *cands;
  localSdp_->isFingerprint = true;
  localSdp_->fingerprint = getMyFingerprint();
  if (nice_->iceState >= NICE_CANDIDATES_GATHERED) {
    cands = nice_->localCandidates;
    cout << " Candidates: " << cands->size() << endl;
    for (unsigned int it = 0; it < cands->size(); it++) {
      CandidateInfo cand = cands->at(it);
      cand.isBundle = bundle_;
      // TODO Check if bundle
      localSdp_->addCandidate(cand);
      if (cand.isBundle) {
        printf("Adding bundle candidate! %d\n", cand.mediaType);
        cand.mediaType = AUDIO_TYPE;
        localSdp_->addCandidate(cand);
      }
    }
  }
  cout << "Processed Local SDP in DTLS Transport" << endl;
}

bool DtlsTransport::isDtlsPacket(const char* buf, int len) {
    int data = DtlsFactory::demuxPacket(reinterpret_cast<const unsigned char*>(buf),len);
    switch(data)
    {
    case DtlsFactory::dtls:
       return true;
       break;
    default:
       return false;
       break;
    }
}
