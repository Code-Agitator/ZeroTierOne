#ifdef ZT_CONTROLLER_USE_LIBPQ
#include "PubSubListener.hpp"

#include "rustybits.h"

namespace ZeroTier {

void listener_callback(void* user_ptr, const uint8_t* payload, uintptr_t length)
{
	if (! user_ptr || ! payload || length == 0) {
		fprintf(stderr, "Invalid parameters in listener_callback\n");
		return;
	}

	auto* listener = static_cast<PubSubListener*>(user_ptr);
	std::string payload_str(reinterpret_cast<const char*>(payload), length);
	listener->onNotification(payload_str);
}

NetworkListener::NetworkListener(const char* controller_id, uint64_t listen_timeout, rustybits::NetworkListenerCallback callback) : _listener(nullptr)
{
	_listener = rustybits::network_listener_new(controller_id, listen_timeout, callback, this);
	_listenThread = std::thread(&NetworkListener::listenThread, this);
	_changeHandlerThread = std::thread(&NetworkListener::changeHandlerThread, this);
}

NetworkListener::~NetworkListener()
{
	if (_listener) {
		rustybits::network_listener_delete(_listener);
		_listener = nullptr;
	}
}

void NetworkListener::onNotification(const std::string& payload)
{
	fprintf(stderr, "Network notification received: %s\n", payload.c_str());
	// TODO: Implement the logic to handle network notifications
}

void NetworkListener::listenThread()
{
	if (_listener) {
		while (rustybits::network_listener_listen(_listener)) {
			// just keep looping
		}
	}
}

void NetworkListener::changeHandlerThread()
{
	if (_listener) {
		rustybits::network_listener_change_handler(_listener);
	}
}

MemberListener::MemberListener(const char* controller_id, uint64_t listen_timeout, rustybits::NetworkListenerCallback callback) : _listener(nullptr)
{
	// Initialize the member listener with the provided controller ID and timeout
	// The callback will be called when a member notification is received
	{
		_listener = rustybits::member_listener_new("controller_id", 60, listener_callback, this);
	}
}

MemberListener::~MemberListener()
{
	if (_listener) {
		rustybits::member_listener_delete(_listener);
		_listener = nullptr;
	}
}

void MemberListener::onNotification(const std::string& payload)
{
	fprintf(stderr, "Member notification received: %s\n", payload.c_str());

	// TODO: Implement the logic to handle network notifications
}

void MemberListener::listenThread()
{
	if (_listener) {
		while (rustybits::member_listener_listen(_listener)) {
			// just keep looping
		}
	}
}

void MemberListener::changeHandlerThread()
{
	if (_listener) {
		rustybits::member_listener_change_handler(_listener);
	}
}

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_USE_LIBPQ