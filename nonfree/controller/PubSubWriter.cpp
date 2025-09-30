#include "PubSubWriter.hpp"

#include "../../osdep/OSUtils.hpp"
#include "CtlUtil.hpp"
#include "member.pb.h"
#include "member_status.pb.h"
#include "network.pb.h"

#include <chrono>
#include <google/cloud/options.h>
#include <google/cloud/pubsub/message.h>
#include <google/cloud/pubsub/publisher.h>
#include <google/cloud/pubsub/topic.h>
#include <opentelemetry/trace/provider.h>

namespace pubsub = ::google::cloud::pubsub;

namespace ZeroTier {

pbmessages::NetworkChange
networkChangeFromJson(std::string controllerID, const nlohmann::json& oldNetwork, const nlohmann::json& newNetwork);
pbmessages::MemberChange
memberChangeFromJson(std::string controllerID, const nlohmann::json& oldMember, const nlohmann::json& newMember);

PubSubWriter::PubSubWriter(std::string project, std::string topic, std::string controller_id)
	: _controller_id(controller_id)
	, _project(project)
	, _topic(topic)
{
	fprintf(
		stderr, "PubSubWriter for controller %s project %s topic %s\n", controller_id.c_str(), project.c_str(),
		topic.c_str());
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// If PUBSUB_EMULATOR_HOST is set, create the topic if it doesn't exist
	const char* emulatorHost = std::getenv("PUBSUB_EMULATOR_HOST");
	if (emulatorHost != nullptr) {
		create_gcp_pubsub_topic_if_needed(project, topic);
	}

	auto options =
		::google::cloud::Options {}
			.set<pubsub::RetryPolicyOption>(pubsub::LimitedTimeRetryPolicy(std::chrono::seconds(5)).clone())
			.set<pubsub::BackoffPolicyOption>(
				pubsub::ExponentialBackoffPolicy(std::chrono::milliseconds(100), std::chrono::seconds(2), 1.3).clone());
	auto publisher = pubsub::MakePublisherConnection(pubsub::Topic(project, topic), std::move(options));
	_publisher = std::make_shared<pubsub::Publisher>(std::move(publisher));
}

PubSubWriter::~PubSubWriter()
{
}

bool PubSubWriter::publishMessage(const std::string& payload, const std::string& frontend)
{
	std::vector<std::pair<std::string, std::string> > attributes;
	attributes.emplace_back("controller_id", _controller_id);

	if (! frontend.empty()) {
		attributes.emplace_back("frontend", frontend);
	}

	auto msg = pubsub::MessageBuilder {}.SetData(payload).SetAttributes(attributes).Build();
	auto message_id = _publisher->Publish(std::move(msg)).get();
	if (! message_id) {
		fprintf(stderr, "Failed to publish message: %s\n", std::move(message_id).status().message().c_str());
		return false;
	}

	fprintf(stderr, "Published message to %s\n", _topic.c_str());
	return true;
}

bool PubSubWriter::publishNetworkChange(
	const nlohmann::json& oldNetwork,
	const nlohmann::json& newNetwork,
	const std::string& frontend)
{
	pbmessages::NetworkChange nc = networkChangeFromJson(_controller_id, oldNetwork, newNetwork);
	std::string payload;
	if (! nc.SerializeToString(&payload)) {
		fprintf(stderr, "Failed to serialize NetworkChange protobuf message\n");
		return false;
	}

	return publishMessage(payload, frontend);
}

bool PubSubWriter::publishMemberChange(
	const nlohmann::json& oldMember,
	const nlohmann::json& newMember,
	const std::string& frontend)
{
	pbmessages::MemberChange mc = memberChangeFromJson(_controller_id, oldMember, newMember);
	std::string payload;
	if (! mc.SerializeToString(&payload)) {
		fprintf(stderr, "Failed to serialize MemberChange protobuf message\n");
		return false;
	}

	return publishMessage(payload, frontend);
}

bool PubSubWriter::publishStatusChange(
	std::string frontend,
	std::string network_id,
	std::string node_id,
	std::string os,
	std::string arch,
	std::string version,
	int64_t last_seen)
{
	auto provider = opentelemetry::trace::Provider::GetTracerProvider();
	auto tracer = provider->GetTracer("PubSubWriter");
	auto span = tracer->StartSpan("PubSubWriter::publishStatusChange");
	auto scope = tracer->WithActiveSpan(span);

	pbmessages::MemberStatus_MemberStatusMetadata* metadata = new pbmessages::MemberStatus_MemberStatusMetadata();
	metadata->set_controller_id(_controller_id);
	metadata->set_trace_id("");	  // TODO: generate a trace ID

	pbmessages::MemberStatus ms;
	ms.set_network_id(network_id);
	ms.set_member_id(node_id);
	ms.set_os(os);
	ms.set_arch(arch);
	ms.set_version(version);
	ms.set_timestamp(last_seen);
	ms.set_allocated_metadata(metadata);

	std::string payload;
	if (! ms.SerializeToString(&payload)) {
		fprintf(stderr, "Failed to serialize StatusChange protobuf message\n");
		return false;
	}

	return publishMessage(payload, "");
}

pbmessages::NetworkChange_Network* networkFromJson(const nlohmann::json& j)
{
	if (! j.is_object()) {
		return nullptr;
	}

	pbmessages::NetworkChange_Network* n = new pbmessages::NetworkChange_Network();
	try {
		n->set_network_id(j.value("id", ""));
		n->set_name(j.value("name", ""));
		n->set_capabilities(OSUtils::jsonDump(j.value("capabilities", "[]"), -1));
		n->set_creation_time(j.value("creationTime", 0));
		n->set_enable_broadcast(j.value("enableBroadcast", false));

		for (const auto& p : j["ipAssignmentPools"]) {
			if (p.is_object()) {
				auto pool = n->add_assignment_pools();
				pool->set_start_ip(p.value("ipRangeStart", ""));
				pool->set_end_ip(p.value("ipRangeEnd", ""));
			}
		}

		n->set_mtu(j.value("mtu", 2800));
		n->set_multicast_limit(j.value("multicastLimit", 32));
		n->set_is_private(j.value("private", true));
		n->set_remote_trace_level(j.value("remoteTraceLevel", 0));
		n->set_remote_trace_target(j.value("remoteTraceTarget", ""));
		n->set_revision(j.value("revision", 0));

		for (const auto& p : j["routes"]) {
			if (p.is_object()) {
				auto r = n->add_routes();
				r->set_target(p.value("target", ""));
				r->set_via(p.value("via", ""));
			}
		}

		n->set_rules("");
		n->set_tags(OSUtils::jsonDump(j.value("tags", "[]"), -1));

		pbmessages::NetworkChange_IPV4AssignMode* v4am = new pbmessages::NetworkChange_IPV4AssignMode();
		if (j["v4AssignMode"].is_object()) {
			v4am->set_zt(j["v4AssignMode"].value("zt", false));
		}
		n->set_allocated_ipv4_assign_mode(v4am);

		pbmessages::NetworkChange_IPV6AssignMode* v6am = new pbmessages::NetworkChange_IPV6AssignMode();
		if (j["v6AssignMode"].is_object()) {
			v6am->set_zt(j["v6AssignMode"].value("zt", false));
			v6am->set_six_plane(j["v6AssignMode"].value("6plane", false));
			v6am->set_rfc4193(j["v6AssignMode"].value("rfc4193", false));
		}
		n->set_allocated_ipv6_assign_mode(v6am);

		nlohmann::json jdns = j.value("dns", nullptr);
		if (jdns.is_object()) {
			pbmessages::NetworkChange_DNS* dns = new pbmessages::NetworkChange_DNS();
			dns->set_domain(jdns.value("domain", ""));
			for (const auto& s : jdns["servers"]) {
				if (s.is_string()) {
					auto server = dns->add_nameservers();
					*server = s;
				}
			}
			n->set_allocated_dns(dns);
		}

		n->set_sso_enabled(j.value("ssoEnabled", false));
		if (j.value("ssoEnabled", false)) {
			n->set_sso_provider(j.value("provider", ""));
			n->set_sso_client_id(j.value("clientId", ""));
			n->set_sso_authorization_endpoint(j.value("authorizationEndpoint", ""));
			n->set_sso_issuer(j.value("issuer", ""));
			n->set_sso_provider(j.value("provider", ""));
		}

		n->set_rules_source(j.value("rulesSource", ""));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception parsing network JSON: %s\n", e.what());
		delete n;
		return nullptr;
	}

	return n;
}

pbmessages::NetworkChange
networkChangeFromJson(std::string controllerID, const nlohmann::json& oldNetwork, const nlohmann::json& newNetwork)
{
	pbmessages::NetworkChange nc;
	nc.set_allocated_old(networkFromJson(oldNetwork));
	nc.set_allocated_new_(networkFromJson(newNetwork));
	nc.set_change_source(pbmessages::NetworkChange_ChangeSource::NetworkChange_ChangeSource_CONTROLLER);

	pbmessages::NetworkChange_NetworkChangeMetadata* metadata = new pbmessages::NetworkChange_NetworkChangeMetadata();
	metadata->set_controller_id(controllerID);
	metadata->set_trace_id("");	  // TODO: generate a trace ID
	nc.set_allocated_metadata(metadata);

	return nc;
}

pbmessages::MemberChange_Member* memberFromJson(const nlohmann::json& j)
{
	if (! j.is_object()) {
		return nullptr;
	}

	pbmessages::MemberChange_Member* m = new pbmessages::MemberChange_Member();
	try {
		m->set_network_id(j.value("networkId", ""));
		m->set_device_id(j.value("id", ""));
		m->set_identity(j.value("identity", ""));
		m->set_authorized(j.value("authorized", false));
		for (const auto& addr : j.value("ipAssignments", nlohmann::json::array())) {
			if (addr.is_string()) {
				auto a = m->add_ip_assignments();
				*a = addr;
			}
		}
		m->set_active_bridge(j.value("activeBridge", false));
		m->set_tags(OSUtils::jsonDump(j.value("tags", "[]"), -1));
		m->set_capabilities(OSUtils::jsonDump(j.value("capabilities", "[]"), -1));
		m->set_creation_time(j.value("creationTime", 0));
		m->set_no_auto_assign_ips(j.value("noAutoAssignIps", false));
		m->set_revision(j.value("revision", 0));
		m->set_last_authorized_time(j.value("lastAuthorizedTime", 0));
		m->set_last_deauthorized_time(j.value("lastDeauthorizedTime", 0));
		m->set_last_authorized_credential_type(j.value("lastAuthorizedCredentialType", nullptr));
		m->set_last_authorized_credential(j.value("lastAuthorizedCredential", nullptr));
		m->set_version_major(j.value("versionMajor", 0));
		m->set_version_minor(j.value("versionMinor", 0));
		m->set_version_rev(j.value("versionRev", 0));
		m->set_version_protocol(j.value("versionProtocol", 0));
		m->set_remote_trace_level(j.value("remoteTraceLevel", 0));
		m->set_remote_trace_target(j.value("remoteTraceTarget", ""));
		m->set_sso_exempt(j.value("ssoExempt", false));
		m->set_auth_expiry_time(j.value("authExpiryTime", 0));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception parsing member JSON: %s\n", e.what());
		delete m;
		return nullptr;
	}

	return m;
}

pbmessages::MemberChange
memberChangeFromJson(std::string controllerID, const nlohmann::json& oldMember, const nlohmann::json& newMember)
{
	pbmessages::MemberChange mc;
	mc.set_allocated_old(memberFromJson(oldMember));
	mc.set_allocated_new_(memberFromJson(newMember));
	mc.set_change_source(pbmessages::MemberChange_ChangeSource::MemberChange_ChangeSource_CONTROLLER);

	pbmessages::MemberChange_MemberChangeMetadata* metadata = new pbmessages::MemberChange_MemberChangeMetadata();
	metadata->set_controller_id(controllerID);
	metadata->set_trace_id("");	  // TODO: generate a trace ID
	mc.set_allocated_metadata(metadata);

	return mc;
}

}	// namespace ZeroTier