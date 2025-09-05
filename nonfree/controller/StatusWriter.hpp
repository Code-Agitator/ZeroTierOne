#ifndef STATUS_WRITER_HPP
#define STATUS_WRITER_HPP

#include "../../node/InetAddress.hpp"

#include <string>

namespace ZeroTier {

/**
 * Abstract interface for writing status information somewhere.
 *
 * Implementations might write to a database, a file, or something else.
 */
class StatusWriter {
  public:
	virtual ~StatusWriter() = default;

	virtual void updateNodeStatus(
		const std::string& network_id,
		const std::string& node_id,
		const std::string& os,
		const std::string& arch,
		const std::string& version,
		const InetAddress& address,
		int64_t last_seen,
		const std::string& target) = 0;
	virtual size_t queueLength() const = 0;
	virtual void writePending() = 0;
};

struct PendingStatusEntry {
	std::string network_id;
	std::string node_id;
	std::string os;
	std::string arch;
	std::string version;
	InetAddress address;
	int64_t last_seen;
	std::string target;
};

}	// namespace ZeroTier

#endif	 // STATUS_WRITER_HPP
