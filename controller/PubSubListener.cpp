#ifdef ZT_CONTROLLER_USE_LIBPQ
#include "PubSubListener.hpp"

#include "DB.hpp"
#include "opentelemetry/trace/provider.h"
#include "rustybits.h"

#include <nlohmann/json.hpp>

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

PubSubNetworkListener::PubSubNetworkListener(std::string controller_id, uint64_t listen_timeout, DB* db) : _run(true), _controller_id(controller_id), _db(db), _listener(nullptr)
{
	_listener = rustybits::network_listener_new(_controller_id.c_str(), listen_timeout, listener_callback, this);
	_listenThread = std::thread(&PubSubNetworkListener::listenThread, this);
	_changeHandlerThread = std::thread(&PubSubNetworkListener::changeHandlerThread, this);
}

PubSubNetworkListener::~PubSubNetworkListener()
{
	_run = false;
	if (_listenThread.joinable()) {
		_listenThread.join();
	}

	if (_listener) {
		rustybits::network_listener_delete(_listener);
		_listener = nullptr;
	}
}

void PubSubNetworkListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubNetworkListener");
	auto span = tracer->StartSpan("PubSubNetworkListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);
	span->SetAttribute("payload", payload);

	fprintf(stderr, "Network notification received: %s\n", payload.c_str());

	try {
		nlohmann::json j = nlohmann::json::parse(payload);
		nlohmann::json& ov_tmp = j["old"];
		nlohmann::json& nv_tmp = j["new"];
		nlohmann::json oldConfig, newConfig;

		if (ov_tmp.is_object()) {
			// TODO:  copy old configuration to oldConfig
			// changing key names along the way
		}
		if (nv_tmp.is_object()) {
			// TODO:  copy new configuration to newConfig
			// changing key names along the way
		}

		if (oldConfig.is_object() && newConfig.is_object()) {
			// network modification
			std::string nwid = oldConfig["id"].get<std::string>();
			span->SetAttribute("action", "network_change");
			span->SetAttribute("network_id", nwid);
			_db->save(newConfig, _db->isReady());
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new network
			std::string nwid = newConfig["id"];
			span->SetAttribute("network_id", nwid);
			span->SetAttribute("action", "new_network");
			_db->save(newConfig, _db->isReady());
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// network deletion
			std::string nwid = oldConfig["id"];
			span->SetAttribute("action", "delete_network");
			span->SetAttribute("network_id", nwid);

			uint64_t networkId = Utils::hexStrToU64(nwid.c_str());
			if (networkId) {
				_db->eraseNetwork(networkId);
			}
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		fprintf(stderr, "JSON parse error: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception in PubSubNetworkListener: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return;
	}
}

void PubSubNetworkListener::listenThread()
{
	if (_listener) {
		while (_run) {
			rustybits::network_listener_listen(_listener);
		}
	}
}

void PubSubNetworkListener::changeHandlerThread()
{
	if (_listener) {
		rustybits::network_listener_change_handler(_listener);
	}
}

PubSubMemberListener::PubSubMemberListener(std::string controller_id, uint64_t listen_timeout, DB* db) : _run(true), _controller_id(controller_id), _db(db), _listener(nullptr)
{
	_run = true;
	_listener = rustybits::member_listener_new(_controller_id.c_str(), listen_timeout, listener_callback, this);
	_listenThread = std::thread(&PubSubMemberListener::listenThread, this);
	_changeHandlerThread = std::thread(&PubSubMemberListener::changeHandlerThread, this);
}

PubSubMemberListener::~PubSubMemberListener()
{
	_run = false;
	if (_listenThread.joinable()) {
		_listenThread.join();
	}

	if (_listener) {
		rustybits::member_listener_delete(_listener);
		_listener = nullptr;
	}
}

void PubSubMemberListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubMemberListener");
	auto span = tracer->StartSpan("PubSubMemberListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);
	span->SetAttribute("payload", payload);

	fprintf(stderr, "Member notification received: %s\n", payload.c_str());

	try {
		nlohmann::json tmp;
		nlohmann::json old_tmp = tmp["old"];
		nlohmann::json new_tmp = tmp["new"];
		nlohmann::json oldConfig, newConfig;

		if (old_tmp.is_object()) {
			// TODO: copy old configuration to oldConfig
		}

		if (new_tmp.is_object()) {
			// TODO: copy new configuration to newConfig
		}

		if (oldConfig.is_object() && newConfig.is_object()) {
			// member modification
			std::string memberID = oldConfig["id"].get<std::string>();
			std::string networkID = oldConfig["nwid"].get<std::string>();
			span->SetAttribute("action", "member_change");
			span->SetAttribute("member_id", memberID);
			span->SetAttribute("network_id", networkID);
			_db->save(newConfig, _db->isReady());
		}
		else if (newConfig.is_object() && ! oldConfig.is_object()) {
			// new member
			std::string memberID = newConfig["id"].get<std::string>();
			std::string networkID = newConfig["nwid"].get<std::string>();
			span->SetAttribute("action", "new_member");
			span->SetAttribute("member_id", memberID);
			span->SetAttribute("network_id", networkID);
			_db->save(newConfig, _db->isReady());
		}
		else if (! newConfig.is_object() && oldConfig.is_object()) {
			// member deletion
			std::string memberID = oldConfig["id"].get<std::string>();
			std::string networkID = oldConfig["nwid"].get<std::string>();
			span->SetAttribute("action", "delete_member");
			span->SetAttribute("member_id", memberID);
			span->SetAttribute("network_id", networkID);

			uint64_t networkId = Utils::hexStrToU64(networkID.c_str());
			uint64_t memberId = Utils::hexStrToU64(memberID.c_str());
			if (networkId && memberId) {
				_db->eraseMember(networkId, memberId);
			}
		}
	}
	catch (const nlohmann::json::parse_error& e) {
		fprintf(stderr, "JSON parse error: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception in PubSubMemberListener: %s\n", e.what());
		span->SetAttribute("error", e.what());
		span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
		return;
	}
}

void PubSubMemberListener::listenThread()
{
	if (_listener) {
		while (_run) {
			rustybits::member_listener_listen(_listener);
		}
	}
}

void PubSubMemberListener::changeHandlerThread()
{
	if (_listener) {
		rustybits::member_listener_change_handler(_listener);
	}
}

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_USE_LIBPQ