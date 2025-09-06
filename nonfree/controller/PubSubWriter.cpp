#include "PubSubWriter.hpp"

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
	if (! frontend.empty()) {
		attributes.emplace_back("frontend", frontend);
	}
	attributes.emplace_back("controller_id", _controller_id);

	auto msg = pubsub::MessageBuilder {}.SetData(payload).SetAttributes(attributes).Build();
	auto message_id = _publisher->Publish(std::move(msg)).get();
	if (! message_id) {
		fprintf(stderr, "Failed to publish message: %s\n", std::move(message_id).status().message().c_str());
		return false;
	}

	fprintf(stderr, "Published message to %s\n", _topic.c_str());
	return true;
}

bool PubSubWriter::publishNetworkChange(const nlohmann::json& networkJson, const std::string& frontend)
{
	pbmessages::NetworkChange nc;
	// nc.mutable_new_()->CopyFrom(fromJson<pbmessages::NetworkChange_Network>(networkJson));
	std::string payload;
	if (! nc.SerializeToString(&payload)) {
		fprintf(stderr, "Failed to serialize NetworkChange protobuf message\n");
		return false;
	}

	return publishMessage(payload, frontend);
}

bool PubSubWriter::publishMemberChange(const nlohmann::json& memberJson, const std::string& frontend)
{
	pbmessages::MemberChange mc;
	// mc.mutable_new_()->CopyFrom(fromJson<pbmessages::MemberChange_Member>(memberJson));
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

	pbmessages::MemberStatus_MemberStatusMetadata metadata;
	metadata.set_controller_id(_controller_id);
	metadata.set_trace_id("");	 // TODO: generate a trace ID

	pbmessages::MemberStatus ms;
	ms.set_network_id(network_id);
	ms.set_member_id(node_id);
	ms.set_os(os);
	ms.set_arch(arch);
	ms.set_version(version);
	ms.set_timestamp(last_seen);
	ms.set_allocated_metadata(&metadata);

	std::string payload;
	if (! ms.SerializeToString(&payload)) {
		fprintf(stderr, "Failed to serialize StatusChange protobuf message\n");
		return false;
	}

	return publishMessage(payload, frontend);
}

}	// namespace ZeroTier