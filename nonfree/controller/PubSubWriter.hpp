#ifndef ZT_CONTROLLER_PUBSUBWRITER_HPP
#define ZT_CONTROLLER_PUBSUBWRITER_HPP

#include <google/cloud/pubsub/publisher.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace ZeroTier {

class PubSubWriter {
  public:
	PubSubWriter(std::string controller_id, std::string project, std::string topic);
	virtual ~PubSubWriter();

	bool publishNetworkChange(const nlohmann::json& networkJson, const std::string& frontend = "");
	bool publishMemberChange(const nlohmann::json& memberJson, const std::string& frontend = "");
	bool publishStatusChange(
		std::string frontend,
		std::string network_id,
		std::string node_id,
		std::string os,
		std::string arch,
		std::string version,
		int64_t last_seen);

  protected:
	bool publishMessage(const std::string& payload, const std::string& frontend = "");

  private:
	std::string _controller_id;
	std::string _project;
	std::string _topic;
	std::shared_ptr<google::cloud::pubsub::Publisher> _publisher;
};

}	// namespace ZeroTier

#endif	 // ZT_CONTROLLER_PUBSUBWRITER_HPP