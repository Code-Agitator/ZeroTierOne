#ifndef CONTROLLER_CONFIG_HPP
#define CONTROLLER_CONFIG_HPP

#include "Redis.hpp"

#include <string>

namespace ZeroTier {

struct PubSubConfig {
	std::string project_id;
};

struct BigTableConfig {
	std::string project_id;
	std::string instance_id;
	std::string table_id;
};

struct ControllerConfig {
	bool ssoEnabled;
	std::string listenMode;
	std::string statusMode;
	RedisConfig* redisConfig;
	PubSubConfig* pubSubConfig;
	BigTableConfig* bigTableConfig;

	ControllerConfig()
		: ssoEnabled(false)
		, listenMode("")
		, statusMode("")
		, redisConfig(nullptr)
		, pubSubConfig(nullptr)
		, bigTableConfig(nullptr)
	{
	}

	~ControllerConfig()
	{
		if (redisConfig) {
			delete redisConfig;
			redisConfig = nullptr;
		}
		if (pubSubConfig) {
			delete pubSubConfig;
			pubSubConfig = nullptr;
		}
		if (bigTableConfig) {
			delete bigTableConfig;
			bigTableConfig = nullptr;
		}
	}
};

}	// namespace ZeroTier
#endif	 // CONTROLLER_CONFIG_HPP