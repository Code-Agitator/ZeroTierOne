/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#ifndef ZT_SELFAWARENESS_HPP
#define ZT_SELFAWARENESS_HPP

#include "Address.hpp"
#include "Constants.hpp"
#include "Hashtable.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Tracks changes to this peer's real world addresses
 */
class SelfAwareness {
  public:
	SelfAwareness(const RuntimeEnvironment* renv);

	/**
	 * Called when a trusted remote peer informs us of our external network address
	 *
	 * @param reporter ZeroTier address of reporting peer
	 * @param receivedOnLocalAddress Local address on which report was received
	 * @param reporterPhysicalAddress Physical address that reporting peer seems to have
	 * @param myPhysicalAddress Physical address that peer says we have
	 * @param trusted True if this peer is trusted as an authority to inform us of external address changes
	 * @param now Current time
	 */
	void iam(void* tPtr, const Address& reporter, const int64_t receivedOnLocalSocket, const InetAddress& reporterPhysicalAddress, const InetAddress& myPhysicalAddress, bool trusted, int64_t now);

	/**
	 * Return all known external surface addresses reported by peers
	 *
	 * @return A vector of InetAddress(es)
	 */
	std::vector<InetAddress> whoami();

	/**
	 * Clean up database periodically
	 *
	 * @param now Current time
	 */
	void clean(int64_t now);

  private:
	struct PhySurfaceKey {
		Address reporter;
		int64_t receivedOnLocalSocket;
		InetAddress reporterPhysicalAddress;
		InetAddress::IpScope scope;

		PhySurfaceKey() : reporter(), scope(InetAddress::IP_SCOPE_NONE)
		{
		}
		PhySurfaceKey(const Address& r, const int64_t rol, const InetAddress& ra, InetAddress::IpScope s) : reporter(r), receivedOnLocalSocket(rol), reporterPhysicalAddress(ra), scope(s)
		{
		}

		inline unsigned long hashCode() const
		{
			return ((unsigned long)reporter.toInt() + (unsigned long)scope);
		}
		inline bool operator==(const PhySurfaceKey& k) const
		{
			return ((reporter == k.reporter) && (receivedOnLocalSocket == k.receivedOnLocalSocket) && (reporterPhysicalAddress == k.reporterPhysicalAddress) && (scope == k.scope));
		}
	};
	struct PhySurfaceEntry {
		InetAddress mySurface;
		uint64_t ts;
		bool trusted;

		PhySurfaceEntry() : mySurface(), ts(0), trusted(false)
		{
		}
		PhySurfaceEntry(const InetAddress& a, const uint64_t t) : mySurface(a), ts(t), trusted(false)
		{
		}
	};

	const RuntimeEnvironment* RR;

	Hashtable<PhySurfaceKey, PhySurfaceEntry> _phy;
	Mutex _phy_m;
};

}	// namespace ZeroTier

#endif
