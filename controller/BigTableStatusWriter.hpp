#ifndef BIGTABLESTATUSWRITER_HPP
#define BIGTABLESTATUSWRITER_HPP

#include "StatusWriter.hpp"

#include <mutex>
#include <string>

namespace ZeroTier {
class BigTableStatusWriter : public StatusWriter {
  public:
	BigTableStatusWriter(const std::string& project_id, const std::string& instance_id, const std::string& table_id);
	virtual ~BigTableStatusWriter();

	virtual void updateNodeStatus(
		const std::string& network_id,
		const std::string& node_id,
		const std::string& os,
		const std::string& arch,
		const std::string& version,
		const InetAddress& address,
		int64_t last_seen) override;
	virtual size_t queueLength() const override;
	virtual void writePending() override;

  private:
	const std::string _project_id;
	const std::string _instance_id;
	const std::string _table_id;

	mutable std::mutex _lock;
	std::vector<PendingStatusEntry> _pending;
};

}	// namespace ZeroTier

#endif