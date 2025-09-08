#include "ControllerChangeNotifier.hpp"

#include "PubSubWriter.hpp"

namespace ZeroTier {

PubSubChangeNotifier::PubSubChangeNotifier(std::string controllerID, std::string project)
	: ControllerChangeNotifier()
	, _cv1networkChangeWriter(std::make_shared<PubSubWriter>(project, "ctl-to-cv1-network-change-stream", controllerID))
	, _cv1memberChangeWriter(std::make_shared<PubSubWriter>(project, "ctl-to-cv1-member-change-stream", controllerID))
	, _cv2networkChangeWriter(std::make_shared<PubSubWriter>(project, "ctl-to-cv2-network-change-stream", controllerID))
	, _cv2memberChangeWriter(std::make_shared<PubSubWriter>(project, "ctl-to-cv2-member-change-stream", controllerID))
{
}

PubSubChangeNotifier::~PubSubChangeNotifier()
{
}

void PubSubChangeNotifier::notifyNetworkChange(
	const nlohmann::json& oldNetwork,
	const nlohmann::json& newNetwork,
	const std::string& frontend)
{
	if (frontend == "cv1") {
		_cv1networkChangeWriter->publishNetworkChange(oldNetwork, newNetwork);
	}
	else if (frontend == "cv2") {
		_cv2networkChangeWriter->publishNetworkChange(oldNetwork, newNetwork);
	}
	else {
		throw std::runtime_error("Unknown frontend: " + frontend);
	}
}

void PubSubChangeNotifier::notifyMemberChange(
	const nlohmann::json& oldMember,
	const nlohmann::json newMember,
	const std::string& frontend)
{
	if (frontend == "cv1") {
		_cv1memberChangeWriter->publishMemberChange(oldMember, newMember);
	}
	else if (frontend == "cv2") {
		_cv2memberChangeWriter->publishMemberChange(oldMember, newMember);
	}
	else {
		throw std::runtime_error("Unknown frontend: " + frontend);
	}
}

}	// namespace ZeroTier