#ifdef ZT_CONTROLLER_USE_LIBPQ

#ifndef ZT_CONTROLLER_PUBSUBLISTENER_HPP
#define ZT_CONTROLLER_PUBSUBLISTENER_HPP

#include "NotificationListener.hpp"
#include "rustybits.h"

#include <google/cloud/pubsub/admin/subscription_admin_client.h>
#include <google/cloud/pubsub/subscriber.h>
#include <memory>
#include <string>
#include <thread>

namespace ZeroTier {
class DB;

struct PubSubConfig {
	const char* controller_id;
	std::string project;
	std::string topic;
	uint64_t listen_timeout;
};

/**
 * Base class for GCP PubSub listeners
 */
class PubSubListener : public NotificationListener {
  public:
	PubSubListener(std::string controller_id, std::string project, std::string topic);
	virtual ~PubSubListener();

	virtual void onNotification(const std::string& payload) = 0;

  protected:
	std::string _controller_id;
	std::string _project;
	std::string _topic;
	std::string _subscription_id;

  private:
	void subscribe();
	bool _run = false;
	google::cloud::pubsub_admin::SubscriptionAdminClient _adminClient;
	google::cloud::pubsub::Subscription _subscription;
	std::shared_ptr<google::cloud::pubsub::Subscriber> _subscriber;
	google::cloud::future<google::cloud::Status> _session;
	std::thread _subscriberThread;
};

/**
 * Listener for network notifications via GCP PubSub
 */
class PubSubNetworkListener : public PubSubListener {
  public:
	PubSubNetworkListener(std::string controller_id, std::string project, DB* db);
	virtual ~PubSubNetworkListener();

	virtual void onNotification(const std::string& payload) override;

  private:
	DB* _db;
};

/**
 * Listener for member notifications via GCP PubSub
 */
class PubSubMemberListener : public PubSubListener {
  public:
	PubSubMemberListener(std::string controller_id, std::string project, DB* db);
	virtual ~PubSubMemberListener();

	virtual void onNotification(const std::string& payload) override;

  private:
	DB* _db;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_PUBSUBLISTENER_HPP
#endif	 // ZT_CONTROLLER_USE_LIBPQ