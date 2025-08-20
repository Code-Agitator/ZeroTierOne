#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_PUBSUBLISTENER_HPP
#define ZT_CONTROLLER_PUBSUBLISTENER_HPP

#include "rustybits.h"

#include <memory>
#include <string>
#include <thread>

namespace ZeroTier {
class PubSubListener {
  public:
	virtual ~PubSubListener()
	{
	}

	virtual void onNotification(const std::string& payload) = 0;
};

class PubSubNetworkListener : public PubSubListener {
  public:
	PubSubNetworkListener(const char* controller_id, uint64_t listen_timeout, rustybits::NetworkListenerCallback callback);
	virtual ~PubSubNetworkListener();

	virtual void onNotification(const std::string& payload) override;

  private:
	void listenThread();
	void changeHandlerThread();

	const rustybits::NetworkListener* _listener;
	std::thread _listenThread;
	std::thread _changeHandlerThread;
};

class PubSubMemberListener : public PubSubListener {
  public:
	PubSubMemberListener(const char* controller_id, uint64_t listen_timeout, rustybits::MemberListenerCallback callback);
	virtual ~PubSubMemberListener();

	virtual void onNotification(const std::string& payload) override;

  private:
	void listenThread();
	void changeHandlerThread();

	const rustybits::MemberListener* _listener;
	std::thread _listenThread;
	std::thread _changeHandlerThread;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_PUBSUBLISTENER_HPP
#endif	 // ZT_CONTROLLER_USE_LIBPQ