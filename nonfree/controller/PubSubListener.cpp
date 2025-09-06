#ifdef ZT_CONTROLLER_USE_LIBPQ
#include "PubSubListener.hpp"

#include "ControllerConfig.hpp"
#include "CtlUtil.hpp"
#include "DB.hpp"
#include "member.pb.h"
#include "network.pb.h"
#include "opentelemetry/trace/provider.h"
#include "rustybits.h"

#include <google/cloud/pubsub/admin/subscription_admin_client.h>
#include <google/cloud/pubsub/admin/subscription_admin_connection.h>
#include <google/cloud/pubsub/admin/topic_admin_client.h>
#include <google/cloud/pubsub/message.h>
#include <google/cloud/pubsub/subscriber.h>
#include <google/cloud/pubsub/subscription.h>
#include <google/cloud/pubsub/topic.h>
#include <nlohmann/json.hpp>

namespace pubsub = ::google::cloud::pubsub;
namespace pubsub_admin = ::google::cloud::pubsub_admin;

namespace ZeroTier {

nlohmann::json toJson(const pbmessages::NetworkChange_Network& nc, pbmessages::NetworkChange_ChangeSource source);
nlohmann::json toJson(const pbmessages::MemberChange_Member& mc, pbmessages::MemberChange_ChangeSource source);

PubSubListener::PubSubListener(std::string controller_id, std::string project, std::string topic)
	: _controller_id(controller_id)
	, _project(project)
	, _topic(topic)
	, _subscription_id("sub-" + controller_id + "-" + topic)
	, _run(false)
	, _adminClient(pubsub_admin::MakeSubscriptionAdminConnection())
	, _subscription(pubsub::Subscription(_project, _subscription_id))
{
	fprintf(
		stderr, "PubSubListener for controller %s project %s topic %s subscription %s\n", controller_id.c_str(),
		project.c_str(), topic.c_str(), _subscription_id.c_str());
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// If PUBSUB_EMULATOR_HOST is set, create the topic if it doesn't exist
	const char* emulatorHost = std::getenv("PUBSUB_EMULATOR_HOST");
	if (emulatorHost != nullptr) {
		create_gcp_pubsub_topic_if_needed(project, topic);
	}

	google::pubsub::v1::Subscription request;
	request.set_name(_subscription.FullName());
	request.set_topic(pubsub::Topic(project, topic).FullName());
	request.set_filter("(attributes.controller_id=\"" + _controller_id + "\")");
	auto sub = _adminClient.CreateSubscription(request);
	if (! sub.ok()) {
		fprintf(stderr, "Failed to create subscription: %s\n", sub.status().message().c_str());
		throw std::runtime_error("Failed to create subscription");
	}

	if (sub.status().code() == google::cloud::StatusCode::kAlreadyExists) {
		fprintf(stderr, "Subscription already exists\n");
		throw std::runtime_error("Subscription already exists");
	}

	_subscriber = std::make_shared<pubsub::Subscriber>(pubsub::MakeSubscriberConnection(_subscription));

	_run = true;
	_subscriberThread = std::thread(&PubSubListener::subscribe, this);
}

PubSubListener::~PubSubListener()
{
	_run = false;
	if (_subscriberThread.joinable()) {
		_subscriberThread.join();
	}
}

void PubSubListener::subscribe()
{
	while (_run) {
		try {
			fprintf(stderr, "Starting new subscription session\n");
			auto session = _subscriber->Subscribe([this](pubsub::Message const& m, pubsub::AckHandler h) {
				auto provider = opentelemetry::trace::Provider::GetTracerProvider();
				auto tracer = provider->GetTracer("PubSubListener");
				auto span = tracer->StartSpan("PubSubListener::onMessage");
				auto scope = tracer->WithActiveSpan(span);
				span->SetAttribute("message_id", m.message_id());
				span->SetAttribute("ordering_key", m.ordering_key());
				span->SetAttribute("attributes", m.attributes().size());

				fprintf(stderr, "Received message %s\n", m.message_id().c_str());
				onNotification(m.data());
				std::move(h).ack();
				span->SetStatus(opentelemetry::trace::StatusCode::kOk);
				return true;
			});

			auto result = session.wait_for(std::chrono::seconds(10));
			if (result == std::future_status::timeout) {
				session.cancel();
				std::this_thread::sleep_for(std::chrono::seconds(5));
				continue;
			}

			if (! session.valid()) {
				fprintf(stderr, "Subscription session no longer valid\n");
				std::this_thread::sleep_for(std::chrono::seconds(5));
				continue;
			}
		}
		catch (google::cloud::Status const& status) {
			fprintf(stderr, "Subscription terminated with status: %s\n", status.message().c_str());
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
	}
}

PubSubNetworkListener::PubSubNetworkListener(std::string controller_id, std::string project, DB* db)
	: PubSubListener(controller_id, project, "controller-network-change-stream")
	, _db(db)
{
}

PubSubNetworkListener::~PubSubNetworkListener()
{
}

void PubSubNetworkListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubNetworkListener");
	auto span = tracer->StartSpan("PubSubNetworkListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::NetworkChange nc;
	if (! nc.ParseFromString(payload)) {
		fprintf(stderr, "Failed to parse NetworkChange protobuf message\n");
		span->SetAttribute("error", "Failed to parse NetworkChange protobuf message");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Failed to parse protobuf");
		return;
	}

	fprintf(stderr, "Network notification received");

	try {
		nlohmann::json oldConfig, newConfig;

		if (nc.has_old()) {
			oldConfig = toJson(nc.old(), nc.change_source());
		}

		if (nc.has_new_()) {
			newConfig = toJson(nc.new_(), nc.change_source());
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

PubSubMemberListener::PubSubMemberListener(std::string controller_id, std::string project, DB* db)
	: PubSubListener(controller_id, project, "controller-member-change-stream")
	, _db(db)
{
}

PubSubMemberListener::~PubSubMemberListener()
{
}

void PubSubMemberListener::onNotification(const std::string& payload)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubMemberListener");
	auto span = tracer->StartSpan("PubSubMemberListener::onNotification");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::MemberChange mc;
	if (! mc.ParseFromString(payload)) {
		fprintf(stderr, "Failed to parse MemberChange protobuf message\n");
		span->SetAttribute("error", "Failed to parse MemberChange protobuf message");
		span->SetStatus(opentelemetry::trace::StatusCode::kError, "Failed to parse protobuf");
		return;
	}

	fprintf(stderr, "Member notification received");

	try {
		nlohmann::json tmp;
		nlohmann::json oldConfig, newConfig;

		if (mc.has_old()) {
			oldConfig = toJson(mc.old(), mc.change_source());
		}

		if (mc.has_new_()) {
			newConfig = toJson(mc.new_(), mc.change_source());
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

nlohmann::json toJson(const pbmessages::NetworkChange_Network& nc, pbmessages::NetworkChange_ChangeSource source)
{
	nlohmann::json out;

	out["id"] = nc.network_id();
	out["name"] = nc.name();
	out["capabilities"] = OSUtils::jsonParse(nc.capabilities());
	out["mtu"] = nc.mtu();
	out["multicastLimit"] = nc.multicast_limit();
	out["private"] = nc.is_private();
	out["remoteTraceLevel"] = nc.remote_trace_level();
	if (nc.has_remote_trace_target()) {
		out["remoteTraceTarget"] = nc.remote_trace_target();
	}
	else {
		out["remoteTraceTarget"] = "";
	}
	out["rules"] = OSUtils::jsonParse(nc.rules());
	out["rulesSource"] = nc.rules_source();
	out["tags"] = OSUtils::jsonParse(nc.tags());

	if (nc.has_ipv4_assign_mode()) {
		nlohmann::json ipv4mode;
		ipv4mode["zt"] = nc.ipv4_assign_mode().zt();
		out["ipv4AssignMode"] = ipv4mode;
	}
	if (nc.has_ipv6_assign_mode()) {
		nlohmann::json ipv6mode;
		ipv6mode["6plane"] = nc.ipv6_assign_mode().six_plane();
		ipv6mode["rfc4193"] = nc.ipv6_assign_mode().rfc4193();
		ipv6mode["zt"] = nc.ipv6_assign_mode().zt();
		out["ipv6AssignMode"] = ipv6mode;
	}

	if (nc.assignment_pools_size() > 0) {
		nlohmann::json pools = nlohmann::json::array();
		for (const auto& p : nc.assignment_pools()) {
			nlohmann::json pool;
			pool["ipRangeStart"] = p.start_ip();
			pool["ipRangeEnd"] = p.end_ip();
			pools.push_back(pool);
		}
		out["assignmentPools"] = pools;
	}

	if (nc.routes_size() > 0) {
		nlohmann::json routes = nlohmann::json::array();
		for (const auto& r : nc.routes()) {
			nlohmann::json route;
			route["target"] = r.target();
			if (r.has_via()) {
				route["via"] = r.via();
			}
			routes.push_back(route);
		}
		out["routes"] = routes;
	}

	if (nc.has_dns()) {
		nlohmann::json dns;
		if (nc.dns().nameservers_size() > 0) {
			nlohmann::json servers = nlohmann::json::array();
			for (const auto& s : nc.dns().nameservers()) {
				servers.push_back(s);
			}
			dns["servers"] = servers;
		}
		dns["domain"] = nc.dns().domain();

		out["dns"] = dns;
	}

	out["ssoEnabled"] = nc.sso_enabled();
	nlohmann::json sso;
	if (nc.sso_enabled()) {
		sso = nlohmann::json::object();
		if (nc.has_sso_client_id()) {
			sso["ssoClientId"] = nc.sso_client_id();
		}

		if (nc.has_sso_authorization_endpoint()) {
			sso["ssoAuthorizationEndpoint"] = nc.sso_authorization_endpoint();
		}

		if (nc.has_sso_issuer()) {
			sso["ssoIssuer"] = nc.sso_issuer();
		}

		if (nc.has_sso_provider()) {
			sso["ssoProvider"] = nc.sso_provider();
		}
	}
	out["ssoConfig"] = sso;
	switch (source) {
		case pbmessages::NetworkChange_ChangeSource_CV1:
			out["change_source"] = "cv1";
			break;
		case pbmessages::NetworkChange_ChangeSource_CV2:
			out["change_source"] = "cv2";
			break;
		case pbmessages::NetworkChange_ChangeSource_CONTROLLER:
			out["change_source"] = "controller";
			break;
		default:
			out["change_source"] = "unknown";
			break;
	}

	return out;
}

nlohmann::json toJson(const pbmessages::MemberChange_Member& mc, pbmessages::MemberChange_ChangeSource source)
{
	nlohmann::json out;
	out["id"] = mc.device_id();
	out["nwid"] = mc.network_id();
	if (mc.has_remote_trace_target()) {
		out["remoteTraceTarget"] = mc.remote_trace_target();
	}
	else {
		out["remoteTraceTarget"] = "";
	}
	out["authorized"] = mc.authorized();
	out["activeBridge"] = mc.active_bridge();

	auto ipAssignments = mc.ip_assignments();
	if (ipAssignments.size() > 0) {
		nlohmann::json assignments = nlohmann::json::array();
		for (const auto& ip : ipAssignments) {
			assignments.push_back(ip);
		}
		out["ipAssignments"] = assignments;
	}

	out["noAutoAssignIps"] = mc.no_auto_assign_ips();
	out["ssoExempt"] = mc.sso_exepmt();
	out["authenticationExpiryTime"] = mc.auth_expiry_time();
	out["capabilities"] = OSUtils::jsonParse(mc.capabilities());
	out["creationTime"] = mc.creation_time();
	out["identity"] = mc.identity();
	out["lastAuthorizedTime"] = mc.last_authorized_time();
	out["lastDeauthorizedTime"] = mc.last_deauthorized_time();
	out["remoteTraceLevel"] = mc.remote_trace_level();
	out["revision"] = mc.revision();
	out["tags"] = OSUtils::jsonParse(mc.tags());
	out["versionMajor"] = mc.version_major();
	out["versionMinor"] = mc.version_minor();
	out["versionRev"] = mc.version_rev();
	out["versionProtocol"] = mc.version_protocol();
	switch (source) {
		case pbmessages::MemberChange_ChangeSource_CV1:
			out["change_source"] = "cv1";
			break;
		case pbmessages::MemberChange_ChangeSource_CV2:
			out["change_source"] = "cv2";
			break;
		case pbmessages::MemberChange_ChangeSource_CONTROLLER:
			out["change_source"] = "controller";
			break;
		default:
			out["change_source"] = "unknown";
			break;
	}

	return out;
}

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_USE_LIBPQ