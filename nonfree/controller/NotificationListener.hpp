#ifndef NOTIFICATION_LISTENER_HPP
#define NOTIFICATION_LISTENER_HPP

#include <string>

namespace ZeroTier {

/**
 * Base class for notification listeners
 *
 * This class is used to receive notifications from various sources such as Redis, PostgreSQL, etc.
 */
class NotificationListener {
  public:
	NotificationListener() = default;
	virtual ~NotificationListener()
	{
	}

	/**
	 * Called when a notification is received.
	 *
	 * Payload should be parsed and passed to the database handler's save method.
	 *
	 * @param payload The payload of the notification.
	 */
	virtual bool onNotification(const std::string& payload) = 0;
};

}	// namespace ZeroTier

#endif	 // NOTIFICATION_LISTENER_HPP