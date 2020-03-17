# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

### [1.0.10](https://github.com/mapequation/infomap/compare/v1.0.9...v1.0.10) (2020-03-17)


### Bug Fixes

* Handle unassigned nodes after input tree ([d337535](https://github.com/mapequation/infomap/commit/d33753500939912035e30db4b4dfaf38beeb3eb1)), closes [#119](https://github.com/mapequation/infomap/issues/119)

### [1.0.9](https://github.com/mapequation/infomap/compare/v1.0.8...v1.0.9) (2020-03-14)


### Bug Fixes

* **windows:** compilation error on std::min ([b6c1a41](https://github.com/mapequation/infomap/commit/b6c1a419ad7f732520723a5df621eaed4bb5d2da))

### [1.0.8](https://github.com/mapequation/infomap/compare/v1.0.7...v1.0.8) (2020-03-12)


### Bug Fixes

* Fix reconstruct physically merged state nodes ([dc91e1a](https://github.com/mapequation/infomap/commit/dc91e1ad93fe150c58994c9dd2ab3aebefd84ef8)), closes [#118](https://github.com/mapequation/infomap/issues/118)
* Remove debug output ([#117](https://github.com/mapequation/infomap/issues/117)) ([60e7ab3](https://github.com/mapequation/infomap/commit/60e7ab346f50d3cedff06a6b7a12ef106b296f5a))

### [1.0.7](https://github.com/mapequation/infomap/compare/v1.0.6...v1.0.7) (2020-03-09)


### Bug Fixes

* Add header in network and states output ([c415a4b](https://github.com/mapequation/infomap/commit/c415a4bf05625eaa2330e6e9e3c9b1e2797895c5))
* Use *Edges/*Arcs instead of *Links in pajek output ([ff3a98e](https://github.com/mapequation/infomap/commit/ff3a98e18c119a64b8324018920452b6611b77d2))
* Use node_id instead of id in clu output ([39cd4e4](https://github.com/mapequation/infomap/commit/39cd4e4d866ee03c1d701946225a2bd76a0a9790))
* **js:** Fix support for all file io in Infomap.js ([ea9d899](https://github.com/mapequation/infomap/commit/ea9d899bdebdd361f0ff90e673471f92bc46cfaa))

### [1.0.6](https://github.com/mapequation/infomap/compare/v1.0.5...v1.0.6) (2020-03-03)


### Bug Fixes

* **python:** Enable numpy.int64 in link weights ([8d91206](https://github.com/mapequation/infomap/commit/8d9120681f932bd0cfad37cc4740f719f2acb358)), closes [#107](https://github.com/mapequation/infomap/issues/107)
* **python:** Print tree by default with python cli ([1565cab](https://github.com/mapequation/infomap/commit/1565cab1f3afec8c555180dd620379557709d3d0)), closes [#106](https://github.com/mapequation/infomap/issues/106)

### [1.0.5](https://github.com/mapequation/infomap/compare/v1.0.4...v1.0.5) (2020-03-02)


### Bug Fixes

* Common parameters should not be advanced ([#101](https://github.com/mapequation/infomap/issues/101)) ([2907c86](https://github.com/mapequation/infomap/commit/2907c8605313d60fc7c74e36d7a841b2f96c7b1d)), closes [#100](https://github.com/mapequation/infomap/issues/100)
* consistently use 1-based indexing for paths and 0 for indexes ([f283818](https://github.com/mapequation/infomap/commit/f2838189771cfc118ee20152d96694b438168ca8)), closes [#103](https://github.com/mapequation/infomap/issues/103)
* Fix ftree links since remapping path from 1 ([f447b48](https://github.com/mapequation/infomap/commit/f447b4872cb29b651747c08cd919e837353b4354)), closes [#102](https://github.com/mapequation/infomap/issues/102)

### [1.0.4](https://github.com/mapequation/infomap/compare/v1.0.3...v1.0.4) (2020-02-28)


### Bug Fixes

* **js:** Revert attempted worker blob optimization ([c521373](https://github.com/mapequation/infomap/commit/c521373fadaf741a34792ea27cd2efba6813cff9))

### [1.0.3](https://github.com/mapequation/infomap/compare/v1.0.2...v1.0.3) (2020-02-28)


### Bug Fixes

* **js:** Handle Infomap exceptions in js ([7a641f9](https://github.com/mapequation/infomap/commit/7a641f94ddae3df99c2065e77e3a1bc78922e174)), closes [#99](https://github.com/mapequation/infomap/issues/99)
* **python:** Start module id from 1 in get_[multilevel_]modules ([9419798](https://github.com/mapequation/infomap/commit/94197982597df86ae315dd9520ae24c27f67c552))

### [1.0.2](https://github.com/mapequation/infomap/compare/v1.0.1...v1.0.2) (2020-02-28)


### Bug Fixes

* **windows:** Fix compilation error on std::min ([3b000dc](https://github.com/mapequation/infomap/commit/3b000dcea45875e222d69b63ae0c289bcc6efcdc)), closes [#97](https://github.com/mapequation/infomap/issues/97)

### [1.0.1](https://github.com/mapequation/infomap/compare/v1.0.0...v1.0.1) (2020-02-27)


### Bug Fixes

* **python:** Fix missing package_meta on pip install ([dd24b1d](https://github.com/mapequation/infomap/commit/dd24b1d8fa7aa0c0b276dd611c57ef8a110002b8)), closes [#95](https://github.com/mapequation/infomap/issues/95)

## 1.0.0 (2020-02-26)


### ⚠ BREAKING CHANGES

* **python:** Drop support for python 2
* **python:** Rename physicalId to node_id.
Remove initial partition as first argument to run, use initial_partition argument.
Start InfomapIterator.path indexing from one.
* Output header format has changed
* Ftree output format has changed to include enter flow
* Clu output now contains top modules instead of leaf modules
* Tree output is sorted on flow
* Clu module ids starts from 1 instead of 0
* Output contains name of the physical node instead of state id
* Full multilayer format require the *multilayer heading
* Remove undirdir and outdirdir, use --flow-model. Rename min-improment to --core-loop-codelength-thresh. Remove random-loop-limit. Rename tune-iteration-threshold to --tune-iteration-relative-threshold. Rename skip-replace-to-one-module to --prefer-modular-solution.
* Removed --print-state-network, use -o states
* Renamed --set-unidentified-nodes-to-closest-module to
--assing-to-neighbouring-module
* No support for *path input

### Features

* **python:** Add meta info to infomap package ([a2caaf4](https://github.com/mapequation/infomap/commit/a2caaf4ff7f368c237c2752207b097028bacb900)), closes [#63](https://github.com/mapequation/infomap/issues/63)
* Add enter flow to ftree output ([bd3255e](https://github.com/mapequation/infomap/commit/bd3255e61977ce8f1d9e0babb95ec8158710e3a8)), closes [#82](https://github.com/mapequation/infomap/issues/82)
* Show entropy rate in the beginning ([f61692c](https://github.com/mapequation/infomap/commit/f61692c60c681d532f7a45b980f5041199b088b5))
* **python:** Improve Python API ([#87](https://github.com/mapequation/infomap/issues/87)) ([c39cd9f](https://github.com/mapequation/infomap/commit/c39cd9f2a310a10a30e0de83dab95902e8c6aa91))
* Add -o/--output option for comma-separated formats ([a255a18](https://github.com/mapequation/infomap/commit/a255a181f109b51eed3e1cbb82bb5f73f1894dd0))
* Add meta data in output file header ([66ccc64](https://github.com/mapequation/infomap/commit/66ccc64fec74a47a92b053642d7ad5fb63b74df2))
* Add option to print parameters in json ([3a2509d](https://github.com/mapequation/infomap/commit/3a2509d28dec022d5bdd2c8b8a4a5fd87448edab))
* Allow multilayer formats without specifying -i. ([08e09f1](https://github.com/mapequation/infomap/commit/08e09f136321dcf917d0d131d57041d32c923e64))
* Allow one-sided multilayer relax limit ([a116b83](https://github.com/mapequation/infomap/commit/a116b83bcf1240a17aca12a576fe634fd3db91dd)), closes [#68](https://github.com/mapequation/infomap/issues/68)
* Begin module id from 1 instead of 0 in clu output ([2bbea5d](https://github.com/mapequation/infomap/commit/2bbea5d537ef0e68ac167d1f84acbf0888d22fb8))
* Create npm package with Infomap worker, changelog and parameters ([44dae36](https://github.com/mapequation/infomap/commit/44dae36d65e9dccd2462db330298887318bc2463))
* Output name of physical node instead of state id ([dda3277](https://github.com/mapequation/infomap/commit/dda3277a2f97152a5b5011c94e440f9bb5ee9b9b))
* Remove --skip-complete-dangling-memory-nodes ([6909f21](https://github.com/mapequation/infomap/commit/6909f21a3ebc6eb84c5bc10f6f43c1839020ba41))
* Remove support for *path input ([e7be175](https://github.com/mapequation/infomap/commit/e7be175378838b294d64390bd6f59cdc6b8a42f1))
* Rename --set-unidentified-nodes-to-closest-module ([16fdc15](https://github.com/mapequation/infomap/commit/16fdc155d2d1fefbb22af1305f7ae7efcd1f06f4))
* Simplify command line interface ([f537132](https://github.com/mapequation/infomap/commit/f5371328e63d761ccaf6ee1cdce543ced80fe25c))
* Sort tree on flow ([6e27858](https://github.com/mapequation/infomap/commit/6e278584c45367145f57ce6323f6a8a9c2d64ffe)), closes [#71](https://github.com/mapequation/infomap/issues/71)
* Write top modules to .clu, not bottom level ([1e16a3c](https://github.com/mapequation/infomap/commit/1e16a3ccdf15ee6155a22d8d0cd4bc6ebfdeb94f))


### Bug Fixes

* **js:** Add missing this in error handler ([7345cdf](https://github.com/mapequation/infomap/commit/7345cdff8aa2b362df9d6522960fb992f5634f6e))
