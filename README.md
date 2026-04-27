# k4ActsTracking


This repository contains the necessary tools to use ACTS functionality in Key4hep


## Dependencies

* Acts

* DD4hep

* ROOT

* EDM4hep

* Gaudi

* k4FWCore

## Installation

```
source /cvmfs/sw-nightlies.hsf.org/key4hep/setup.sh
```
If needed, build local ACTS with build option:

```
cmake -B . -S ../source -DCMAKE_BUILD_TYPE=Debug -DACTS_BUILD_FATRAS=on -DACTS_BUILD_EXAMPLES=on -DACTS_BUILD_PLUGIN_DD4HEP=on -DACTS_BUILD_PLUGIN_ROOT=on -DACTS_BUILD_EXAMPLES_ROOT=on -DACTS_BUILD_EXAMPLES_PYTHON_BINDINGS=on -DACTS_BUILD_EXAMPLES_GEANT4=on -DACTS_BUILD_EXAMPLES_DD4HEP=on -DACTS_BUILD_PLUGIN_GEANT4=ON -DACTS_BUILD_PLUGIN_JSON=on -DACTS_BUILD_UNITTESTS=on -DCMAKE_INSTALL_PREFIX=../install
```
then export ACTS install path to CMAKE prefix.

```
mkdir build install
cd build;
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make install


```

## Environment

Setup key4hep nightly, ACTS library, luxegeo (geo factory), etc.

```
source env_01.sh
```
