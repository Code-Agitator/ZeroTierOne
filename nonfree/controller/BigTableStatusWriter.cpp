#include "BigTableStatusWriter.hpp"

#include "ControllerConfig.hpp"

#include <google/cloud/bigtable/mutations.h>
#include <google/cloud/bigtable/row.h>
#include <google/cloud/bigtable/table.h>

namespace cbt = google::cloud::bigtable;

namespace ZeroTier {

const std::string nodeInfoColumnFamily = "node_info";
const std::string checkInColumnFamily = "check_in";

const std::string osColumn = "os";
const std::string archColumn = "arch";
const std::string versionColumn = "version";
const std::string ipv4Column = "ipv4";
const std::string ipv6Column = "ipv6";
const std::string lastSeenColumn = "last_seen";

BigTableStatusWriter::BigTableStatusWriter(
	const std::string& project_id,
	const std::string& instance_id,
	const std::string& table_id)
	: _project_id(project_id)
	, _instance_id(instance_id)
	, _table_id(table_id)
{
}

BigTableStatusWriter::~BigTableStatusWriter()
{
	writePending();
}

void BigTableStatusWriter::updateNodeStatus(
	const std::string& network_id,
	const std::string& node_id,
	const std::string& os,
	const std::string& arch,
	const std::string& version,
	const InetAddress& address,
	int64_t last_seen)
{
	std::lock_guard<std::mutex> l(_lock);
	_pending.push_back({ network_id, node_id, os, arch, version, address, last_seen });
	if (_pending.size() >= 100) {
		writePending();
	}
}

size_t BigTableStatusWriter::queueLength() const
{
	std::lock_guard<std::mutex> l(_lock);
	return _pending.size();
}

void BigTableStatusWriter::writePending()
{
	std::vector<PendingStatusEntry> toWrite;
	{
		std::lock_guard<std::mutex> l(_lock);
		toWrite.swap(_pending);
	}
	if (toWrite.empty()) {
		return;
	}

	namespace cbt = google::cloud::bigtable;
	cbt::Table table(cbt::MakeDataConnection(), cbt::TableResource(_project_id, _instance_id, _table_id));

	cbt::BulkMutation bulk;
	for (const auto& entry : toWrite) {
		std::string row_key = entry.network_id + "#" + entry.node_id;
		cbt::SingleRowMutation m(row_key);
		m.emplace_back(cbt::SetCell(nodeInfoColumnFamily, osColumn, entry.os));
		m.emplace_back(cbt::SetCell(nodeInfoColumnFamily, archColumn, entry.arch));
		m.emplace_back(cbt::SetCell(nodeInfoColumnFamily, versionColumn, entry.version));
		char buf[64] = { 0 };
		if (entry.address.ss_family == AF_INET) {
			m.emplace_back(cbt::SetCell(checkInColumnFamily, ipv4Column, entry.address.toString(buf)));
		}
		else if (entry.address.ss_family == AF_INET6) {
			m.emplace_back(cbt::SetCell(checkInColumnFamily, ipv6Column, entry.address.toString(buf)));
		}
		int64_t ts = entry.last_seen;
		m.emplace_back(cbt::SetCell(checkInColumnFamily, lastSeenColumn, std::move(ts)));
		bulk.push_back(std::move(m));
	}

	std::vector<cbt::FailedMutation> failures = table.BulkApply(bulk);
	for (auto const& r : failures) {
		// Handle error (log it, retry, etc.)
		std::cerr << "Error writing to BigTable: " << r.status() << "\n";
	}
}

}	// namespace ZeroTier