/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

#include "Capability.hpp"

#include "Identity.hpp"
#include "Network.hpp"
#include "Node.hpp"
#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Topology.hpp"

namespace ZeroTier {

int Capability::verify(const RuntimeEnvironment* RR, void* tPtr) const
{
	try {
		// There must be at least one entry, and sanity check for bad chain max length
		if ((_maxCustodyChainLength < 1) || (_maxCustodyChainLength > ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH)) {
			return -1;
		}

		// Validate all entries in chain of custody
		Buffer<(sizeof(Capability) * 2)> tmp;
		this->serialize(tmp, true);
		for (unsigned int c = 0; c < _maxCustodyChainLength; ++c) {
			if (c == 0) {
				if ((! _custody[c].to) || (! _custody[c].from) || (_custody[c].from != Network::controllerFor(_nwid))) {
					return -1;	 // the first entry must be present and from the network's controller
				}
			}
			else {
				if (! _custody[c].to) {
					return 0;	// all previous entries were valid, so we are valid
				}
				else if ((! _custody[c].from) || (_custody[c].from != _custody[c - 1].to)) {
					return -1;	 // otherwise if we have another entry it must be from the previous holder in the chain
				}
			}

			const Identity id(RR->topology->getIdentity(tPtr, _custody[c].from));
			if (id) {
				if (! id.verify(tmp.data(), tmp.size(), _custody[c].signature)) {
					return -1;
				}
			}
			else {
				RR->sw->requestWhois(tPtr, RR->node->now(), _custody[c].from);
				return 1;
			}
		}

		// We reached max custody chain length and everything was valid
		return 0;
	}
	catch (...) {
	}
	return -1;
}

}	// namespace ZeroTier
