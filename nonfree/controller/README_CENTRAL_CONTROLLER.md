# Central Controller Builds

NOTE: for ZeroTier, Inc Internal use only.  We do not support these builds for external use, nor do we guarantee this will work for anyone but us.

## Prerequisites

`cmake` is used for builds and `conda` is used to manage external dependencies.

First, install `conda`:

```bash
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -o /tmp/miniconda.sh
bash /tmp/miniconda.sh -b -u -p $HOME/miniconda3
```

Initialize conda:

```bash
source ~/miniconda3/bin/activate
conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/main
conda tos accept --override-channels --channel https://repo.anaconda.com/pkgs/r
conda config --set channel_priority strict
```

Install external dependencies:

```bash
conda install -y -c conda-forge \
    conda-forge::cmake \
    conda-forge::git \
    conda-forge::cxx-compiler \
    conda-forge::c-compiler \
    conda-forge::make \
    conda-forge::pkg-config \
    conda-forge::libpqxx=7.7.3 \
    conda-forge::libopentelemetry-cpp=1.21.0 \
    conda-forge::libopentelemetry-cpp-headers=1.21.0 \
    conda-forge::google-cloud-cpp=2.39.0 \
    conda-forge::libgoogle-cloud=2.39.0 \
    conda-forge::rust=1.89.0 \
    conda-forge::inja=3.3.0 \
    conda-forge::libhiredis=1.3.0 \
    conda-forge::nlohmann_json=3.12.0
```

## Build the Central Controller Binary


```bash
cmake -DCMAKE_BUILD_TYPE=Release -DZT1_CENTRAL_CONTROLLER=1 -DCMAKE_INSTALL_PREFIX=$PWD/out -S . -B build/ -DCMAKE_INSTALL_PREFIX=$(shell pwd)/build-out
cmake --build build/ --target all -j8 --verbose
```

## Packaging via Docker

TODO: write me


## Configuration

Central Controller has new configuration options outside of the normal "settings" block of `local.conf`.

```json
{
  "settings": { 
    ...standard zt1 local.conf settings... 
  },
  "controller": {
    "listenMode": (pgsql|redis|pubsub),
    "statusMode": (pgsql|redis|bigtable),
    "redis": {
      "hostname": ...,
      "port": 6379,
      "clusterMode": true
    },
    "pubsub": {
      "project_id": <gcp-project-id>
    },
    "bigtable": {
      "project_id": <gcp-project-id>,
      "instance_id": <bigtable-instance-id>,
      "table_id": <bigtable-table-id>
    }
  }
}
```

Configuration checks for invalid configurations like `listenMode = "pubsub"`, but without a `"pubsub"` config block.
