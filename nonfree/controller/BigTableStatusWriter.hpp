#ifndef BIGTABLESTATUSWRITER_HPP
#define BIGTABLESTATUSWRITER_HPP

#include "StatusWriter.hpp"

#include <google/cloud/bigtable/table.h>
#include <memory>
#include <mutex>
#include <string>

namespace ZeroTier {

class PubSubWriter;

class BigTableStatusWriter : public StatusWriter {
  public:
	BigTableStatusWriter(
		const std::string& project_id,
		const std::string& instance_id,
		const std::string& table_id,
		std::shared_ptr<PubSubWriter> pubsubWriter);
	virtual ~BigTableStatusWriter();

	virtual void updateNodeStatus(
		const std::string& network_id,
		const std::string& node_id,
		const std::string& os,
		const std::string& arch,
		const std::string& version,
		const InetAddress& address,
		int64_t last_seen,
		const std::string& frontend) override;
	virtual size_t queueLength() const override;
	virtual void writePending() override;

  private:
	const std::string _project_id;
	const std::string _instance_id;
	const std::string _table_id;

	mutable std::mutex _lock;
	std::vector<PendingStatusEntry> _pending;
	std::shared_ptr<PubSubWriter> _pubsubWriter;
	google::cloud::bigtable::Table* _table;
};

}	// namespace ZeroTier

#endif