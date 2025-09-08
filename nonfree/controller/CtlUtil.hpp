/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifndef ZT_CTLUTIL_HPP
#define ZT_CTLUTIL_HPP

#include <string>
#include <vector>

namespace ZeroTier {

const char* _timestr();

std::vector<std::string> split(std::string str, char delim);

std::string url_encode(const std::string& value);

std::string random_hex_string(std::size_t length)

#ifdef ZT1_CENTRAL_CONTROLLER
	void create_gcp_pubsub_topic_if_needed(std::string project_id, std::string topic_id);
#endif

}	// namespace ZeroTier

#endif	 // namespace ZeroTier
