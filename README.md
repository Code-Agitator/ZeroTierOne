ZeroTier - Global Area Networking
======

# ZeroTier \- Global Area Networking

ZeroTier is a smart programmable Ethernet switch for planet Earth. It allows all networked devices, VMs, containers, and applications to communicate as if they all reside in the same physical data center or cloud region.

This is accomplished by combining a cryptographically addressed and secure peer-to-peer network (termed VL1) with an Ethernet emulation layer somewhat similar to VXLAN (termed VL2). Our VL2 Ethernet virtualization layer includes advanced enterprise SDN features like fine grained access control rules for network micro-segmentation and security monitoring.

All ZeroTier traffic is encrypted end-to-end using secret keys that only you control. Most traffic flows peer-to-peer, though we offer free (but slow) relaying for users who cannot establish peer-to-peer connections.

Visit [ZeroTier’s site](https://www.zerotier.com/) or [ZeroTier's documentation](https://docs.zerotier.com) for more information and [pre-built binary packages](https://www.zerotier.com/download/). Apps for Android and iOS are available for free in the Google Play and Apple app stores.

## Project Layout

* artwork/: icons, logos, etc.

* attic/: old stuff and experimental code that we want to keep around for reference.

* controller/: the reference network controller implementation, which is built and included by default on desktop and server build targets.

* debian/: files for building Debian packages on Linux.

* doc/: manual pages and other documentation.

* ext/: third party libraries, binaries that we ship for convenience on some platforms (Mac and Windows), and installation support files.

* include/: include files for the ZeroTier core.

* java/: a JNI wrapper used with our Android mobile app.

* node/: the ZeroTier virtual Ethernet switch core. Note: do not use C++11 features in here, since we want this to build on old embedded platforms.

* osdep/: code to support and integrate with OSes, including platform-specific stuff only built for certain targets.

* rule-compiler/: JavaScript rules language compiler for defining network-level rules.

* service/: the ZeroTier One service, which wraps the ZeroTier core and provides VPN-like connectivity to virtual networks.

* windows/: Visual Studio solution files, Windows service code, and the Windows task bar app UI.

* zeroidc/: OIDC implementation used by ZeroTier service to log into SSO-enabled networks. (Written in Rust)

## License

ZeroTier is licensed under the [BSL version 1.1](https://mariadb.com/bsl11/). See [LICENSE.txt](http://LICENSE.txt) and the [ZeroTier pricing page](https://www.zerotier.com/pricing) for details. ZeroTier is free to use internally in businesses and academic institutions and for non-commercial purposes. Certain types of commercial use, such as building closed-source apps and devices based on ZeroTier or offering ZeroTier network controllers and network management as a SaaS service, require a commercial license.

A small amount of third-party code is also included in ZeroTier and is not subject to our BSL license. See [AUTHORS.md](http://AUTHORS.md) for a list of third-party code, where it is included, and the licenses that apply to it. All of the third-party code in ZeroTier is liberally licensed (MIT, BSD, Apache, public domain, etc.).

## Additional Resources

* [User Documentation](https://docs.zerotier.com)

* [API Reference](http://service/README.md)

* [Network Controller](http://controller/README.md)

* [Commercial Support](https://www.zerotier.com/contact)