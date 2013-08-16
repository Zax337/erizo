/*
 * OneToManyProcessor.h
 */

#ifndef ONETOMANYPROCESSOR_H_
#define ONETOMANYPROCESSOR_H_

#include <map>
#include <string>
#include <arpa/inet.h>

#define BISTRI_REC 1
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}



#define VP8_PL 0x64 
#define RED_PL 116
#define MAX_RTP_LEN 1200
#define HEADER_SIZE 15

#include "MediaDefinitions.h"

namespace erizo{

class WebRtcConnection;

/*
typedef struct {
    uint8_t b:1;
    uint8_t fi:2;
    uint8_t n:1;
    uint8_t i:1;
    uint8_t RSV:3;
//    uint32_t pID;
} vp8desc;
*/

typedef struct {
    uint8_t x:1;
    uint8_t r:1;
    uint8_t n:1;
    uint8_t s:1;
    uint8_t partID:4;
    void print() {
        printf("VP8header x %u, r %u, n %u, s %u, partID %u\n", x, r, n, s, partID);
    }
} vp8desc;

typedef struct {
    uint8_t i:1;
    uint8_t l:1;
    uint8_t t:1;
    uint8_t k:1;
    uint8_t rsv:4;
    void print() {
        printf("VP8header_x i %u, l %u, t %u, k %u, rsv %u\n", i, l, t, k, rsv);
    }
} vp8desc_x;

typedef struct {
    uint8_t pl:7;
    uint8_t p:1;
    void print() {
        printf("REDheader pl %u, p %u\n", pl, p);
    }
} redhdr;

/**
 * Represents a One to Many connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyProcessor : public MediaSink, public FeedbackSink {
public:
	MediaSource *publisher;
	std::map<std::string, MediaSink*> subscribers;

	OneToManyProcessor();
	virtual ~OneToManyProcessor();
	/**
	 * Sets the Publisher
	 * @param webRtcConn The WebRtcConnection of the Publisher
	 */
	void setPublisher(MediaSource* webRtcConn);
    void updateSsrcs();
	/**
	 * Sets the subscriber
	 * @param webRtcConn The WebRtcConnection of the subscriber
	 * @param peerId An unique Id for the subscriber
	 */
	void addSubscriber(MediaSink* webRtcConn, const std::string& peerId);
	/**
	 * Eliminates the subscriber given its peer id
	 * @param peerId the peerId
	 */
    void removeSubscriber(const std::string& peerId);
    int deliverAudioData(char* buf, int len);
    int deliverVideoData(char* buf, int len);

    int deliverFeedback(char* buf, int len);

    void close();
    void closeSink();
    /**
     * Closes all the subscribers and the publisher, the object is useless after this
     */
    void closeAll();

/*
  void FillRTPHeader(uint8_t * buf, bool marker); 
  void FillREDHeader(uint8_t * buf);
  void FillVP8Header(uint8_t * buf, bool, bool);
  void FillPayload(uint8_t * buf, uint8_t * data, int size);*/
  void SendVideoToSubscribers(uint8_t * data, int size);


private:
	char* sendVideoBuffer_;
    int cursor_;
	char* sendAudioBuffer_;
	unsigned int sentPackets_;
    std::string rtcpReceiverPeerId_;
    FeedbackSink* feedbackSink_;
    uint16_t seqnb_;
    uint32_t timestamp_;
    unsigned int frame_count_;

    AVCodecContext * codec_ctx;
    AVCodec * codec;
    AVFrame * frame;
    AVCodecContext * ecodec_ctx;
    AVCodec * ecodec;
#if BISTRI_REC
    AVFormatContext * oc_;
    int stream_index;
#endif

};

} /* namespace erizo */
#endif /* ONETOMANYPROCESSOR_H_ */
