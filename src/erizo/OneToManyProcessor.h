/*
 * OneToManyProcessor.h
 */

#ifndef ONETOMANYPROCESSOR_H_
#define ONETOMANYPROCESSOR_H_

#include <map>
#include <string>

#include "MediaDefinitions.h"

namespace erizo{

class WebRtcConnection;

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

private:
    char* sendVideoBuffer_;
    char* sendAudioBuffer_;
    unsigned int sentPackets_;
    std::string rtcpReceiverPeerId_;
    FeedbackSink* feedbackSink_;
};

} /* namespace erizo */
#endif /* ONETOMANYPROCESSOR_H_ */
