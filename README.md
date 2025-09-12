ZeroTier - Global Area Networking
======

# ZeroTier \- Global Area Networking

ZeroTier is a smart programmable Ethernet switch for planet Earth. It allows all networked devices, VMs, containers, and applications to communicate as if they all reside in the same physical data center or cloud region.

This is accomplished by combining a cryptographically addressed and secure peer-to-peer network (termed VL1) with an Ethernet emulation layer somewhat similar to VXLAN (termed VL2). Our VL2 Ethernet virtualization layer includes advanced enterprise SDN features like fine grained access control rules for network micro-segmentation and security monitoring.

All ZeroTier traffic is encrypted end-to-end using secret keys that only you control. Most traffic flows peer-to-peer, though we offer free (but slow) relaying for users who cannot establish peer-to-peer connections.

Visit [ZeroTier’s site](https://www.zerotier.com/) or [ZeroTier's documentation](https://docs.zerotier.com) for more information and [pre-built binary packages](https://www.zerotier.com/download/). Apps for Android and iOS are available for free in the Google Play and Apple app stores.

### Build and Platform Notes

To build on Mac and Linux just type `make`. On FreeBSD and OpenBSD `gmake` (GNU make) is required and can be installed from packages or ports. For Windows there is a Visual Studio solution in `windows/`.

 - **Mac**
   - Xcode command line tools for macOS 10.13 or newer are required.
   - Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.
 - **Linux**
   - The minimum compiler versions required are GCC/G++ 8.x or CLANG/CLANG++ 5.x.
   - Linux makefiles automatically detect and prefer clang/clang++ if present as it produces smaller and slightly faster binaries in most cases. You can override by supplying CC and CXX variables on the make command line.
   - Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.
 - **Windows**
   - Visual Studio 2022 on Windows 10 or newer.
   - Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.
 - **FreeBSD**
   - GNU make is required. Type `gmake` to build.
   - `binutils` is required.  Type `pkg install binutils` to install.
   - Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.
 - **OpenBSD**
   - There is a limit of four network memberships on OpenBSD as there are only four tap devices (`/dev/tap0` through `/dev/tap3`).
   - GNU make is required. Type `gmake` to build.
   - Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.

Typing `make selftest` will build a *zerotier-selftest* binary which unit tests various internals and reports on a few aspects of the build environment. It's a good idea to try this on novel platforms or architectures.

### Running

Running *zerotier-one* with `-h` option will show help.

On Linux and BSD, if you built from source, you can start the service with:

    sudo ./zerotier-one -d

On most distributions, macOS, and Windows, the installer will start the service and set it up to start on boot.

A home folder for your system will automatically be created.

The service is controlled via the JSON API, which by default is available at `127.0.0.1:9993`. It also listens on `0.0.0.0:9993` which is only usable if `allowManagementFrom` is properly configured in `local.conf`. We include a *zerotier-cli* command line utility to make API calls for standard things like joining and leaving networks. The *authtoken.secret* file in the home folder contains the secret token for accessing this API. See [service/README.md](service/README.md) for API documentation.

Here's where home folders live (by default) on each OS:

 * **Linux**: `/var/lib/zerotier-one`
 * **FreeBSD** / **OpenBSD**: `/var/db/zerotier-one`
 * **Mac**: `/Library/Application Support/ZeroTier/One`
 * **Windows**: `\ProgramData\ZeroTier\One` (That's the default. The base 'shared app data' folder might be different if Windows is installed with a non-standard drive letter assignment or layout.)

## License

ZeroTier is licensed under the [BSL version 1.1](https://mariadb.com/bsl11/). See [LICENSE.txt](http://LICENSE.txt) and the [ZeroTier pricing page](https://www.zerotier.com/pricing) for details. ZeroTier is free to use internally in businesses and academic institutions and for non-commercial purposes. Certain types of commercial use, such as building closed-source apps and devices based on ZeroTier or offering ZeroTier network controllers and network management as a SaaS service, require a commercial license.

A small amount of third-party code is also included in ZeroTier and is not subject to our BSL license. See [AUTHORS.md](http://AUTHORS.md) for a list of third-party code, where it is included, and the licenses that apply to it. All of the third-party code in ZeroTier is liberally licensed (MIT, BSD, Apache, public domain, etc.).

## Additional Resources

* [User Documentation](https://docs.zerotier.com)

* [API Reference](http://service/README.md)

* [Network Controller](http://controller/README.md)

* [Commercial Support](https://www.zerotier.com/contact)