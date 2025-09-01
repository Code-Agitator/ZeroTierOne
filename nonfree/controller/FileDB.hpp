/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifndef ZT_CONTROLLER_FILEDB_HPP
#define ZT_CONTROLLER_FILEDB_HPP

#include "DB.hpp"

namespace ZeroTier {

class FileDB : public DB {
  public:
	FileDB(const char* path);
	virtual ~FileDB();

	virtual bool waitForReady();
	virtual bool isReady();
	virtual bool save(nlohmann::json& record, bool notifyListeners);
	virtual void eraseNetwork(const uint64_t networkId);
	virtual void eraseMember(const uint64_t networkId, const uint64_t memberId);
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress);
	virtual void nodeIsOnline(const uint64_t networkId, const uint64_t memberId, const InetAddress& physicalAddress, const char* osArch);

  protected:
	std::string _path;
	std::string _networksPath;
	std::string _tracePath;
	std::thread _onlineUpdateThread;
	std::map<uint64_t, std::map<uint64_t, std::map<int64_t, InetAddress> > > _online;
	std::mutex _online_l;
	bool _running;
};

}	// namespace ZeroTier

#endif
