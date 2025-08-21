#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_PUBSUBLISTENER_HPP
#define ZT_CONTROLLER_PUBSUBLISTENER_HPP

#include "NotificationListener.hpp"
#include "rustybits.h"

#include <memory>
#include <string>
#include <thread>

namespace ZeroTier {
class DB;

struct PubSubConfig {
	const char* controller_id;
	uint64_t listen_timeout;
};

/**
 * Base class for GCP PubSub listeners
 */
class PubSubListener : public NotificationListener {
  public:
	virtual ~PubSubListener()
	{
	}

	virtual void onNotification(const std::string& payload) = 0;
};

/**
 * Listener for network notifications via GCP PubSub
 */
class PubSubNetworkListener : public PubSubListener {
  public:
	PubSubNetworkListener(std::string controller_id, uint64_t listen_timeout, DB* db);
	virtual ~PubSubNetworkListener();

	virtual void onNotification(const std::string& payload) override;

  private:
	void listenThread();
	void changeHandlerThread();

	bool _run = false;
	std::string _controller_id;
	DB* _db;
	const rustybits::NetworkListener* _listener;
	std::thread _listenThread;
	std::thread _changeHandlerThread;
};

/**
 * Listener for member notifications via GCP PubSub
 */
class PubSubMemberListener : public PubSubListener {
  public:
	PubSubMemberListener(std::string controller_id, uint64_t listen_timeout, DB* db);
	virtual ~PubSubMemberListener();

	virtual void onNotification(const std::string& payload) override;

  private:
	void listenThread();
	void changeHandlerThread();

	bool _run = false;
	std::string _controller_id;
	DB* _db;
	const rustybits::MemberListener* _listener;
	std::thread _listenThread;
	std::thread _changeHandlerThread;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_PUBSUBLISTENER_HPP
#endif	 // ZT_CONTROLLER_USE_LIBPQ