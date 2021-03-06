#ifndef SDESTRANSPORT_H_
#define SDESTRANSPORT_H_

#include <string.h>
#include "NiceConnection.h"
#include "Transport.h"

namespace erizo {
	class SrtpChannel;
	class SdesTransport : public Transport {
		public:
			SdesTransport(MediaType med, const std::string &transport_name, bool bundle, bool rtcp_mux, const std::string& stunServe, const int stunPort, const std::string& cred_id, const std::string& cred_pass, CryptoInfo *remoteCrypto, TransportListener *transportListener);
			~SdesTransport();
			void connectionStateChanged(IceState newState);
			void onNiceData(unsigned int component_id, char* data, int len, NiceConnection* nice);
			void write(char* data, int len);
      		void updateIceState(IceState state, NiceConnection *conn);
      		void processLocalSdp(SdpInfo *localSdp_);
      		void setRemoteCrypto(CryptoInfo *remoteCrypto);
      		void close();

		private:
			char* protectBuf_, *unprotectBuf_;
			boost::mutex writeMutex_, readMutex_;
			SrtpChannel *srtp_, *srtcp_;
			bool readyRtp, readyRtcp;
			bool bundle_;
			CryptoInfo cryptoLocal_, cryptoRemote_;
			friend class Transport;
	};
}
#endif
