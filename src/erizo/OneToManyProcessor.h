/*
 * OneToManyProcessor.h
 */

#ifndef ONETOMANYPROCESSOR_H_
#define ONETOMANYPROCESSOR_H_

#include <map>

#include "MediaDefinitions.h"
#include "media/MediaProcessor.h"
#include "media/utils/RtpUtils.h"


namespace erizo{
class WebRtcConnection;


/**
 * Represents a One to Many connection.
 * Receives media from one publisher and retransmits it to every subscriber.
 */
class OneToManyProcessor : public MediaReceiver {
public:
	OneToManyProcessor();
	virtual ~OneToManyProcessor();
	/**
	 * Sets the Publisher
	 * @param webRtcConn The WebRtcConnection of the Publisher
	 */
	void setPublisher(WebRtcConnection* webRtcConn);
	/**
	 * Sets the subscriber
	 * @param webRtcConn The WebRtcConnection of the subscriber
	 * @param peerId An unique Id for the subscriber
	 */
	void addSubscriber(WebRtcConnection* webRtcConn, int peerId);
	/**
	 * Eliminates the subscriber given its peer id
	 * @param peerId the peerId
	 */
	void removeSubscriber(int peerId);
	int receiveAudioData(char* buf, int len);
	int receiveVideoData(char* buf, int len);

	WebRtcConnection *publisher;
	MediaProcessor *mp;
	std::map<int, WebRtcConnection*> subscribers;

private:
	char* sendVideoBuffer_;
	char* sendAudioBuffer_;
	char* unpackagedBuffer_;
	char* decodedBuffer_;
	char* codedBuffer_;
	int gotFrame_,gotDecodedFrame_, size_;
	RtpParser pars;

};

} /* namespace erizo */
#endif /* ONETOMANYPROCESSOR_H_ */