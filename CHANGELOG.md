# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

### [1.4.4](https://github.com/mapequation/infomap/compare/v1.4.3...v1.4.4) (2021-06-14)

### [1.4.3](https://github.com/mapequation/infomap/compare/v1.4.2...v1.4.3) (2021-06-14)

### [1.4.2](https://github.com/mapequation/infomap/compare/v1.4.1...v1.4.2) (2021-06-14)


### Bug Fixes

* **build:** Fix building Python package with OpenMP if available ([cfaade5](https://github.com/mapequation/infomap/commit/cfaade5bb699980954fc6d618ef1eb211bb2a63e))

### [1.4.1](https://github.com/mapequation/infomap/compare/v1.4.0...v1.4.1) (2021-06-10)


### Bug Fixes

* **js:** Add newick and json to output from JS worker ([549ee84](https://github.com/mapequation/infomap/commit/549ee84858860680d2b9ed93e2d79a419c24e7d1))
* **python:** Add missing write_newick and write_json methods ([62c2736](https://github.com/mapequation/infomap/commit/62c2736dfb048de432093313e19b3d67869d3138))
* Change Newick output extention to .nwk ([f0e2e8e](https://github.com/mapequation/infomap/commit/f0e2e8ef8b6f5da6985c548b2f2b00521177cad8))

## [1.4.0](https://github.com/mapequation/infomap/compare/v1.3.0...v1.4.0) (2021-06-10)


### Features

* Support printing JSON tree ([22116d6](https://github.com/mapequation/infomap/commit/22116d6a9077909c2e5eb2160feea913022752e8))
* **python:** Accept named arguments ([5dabf4a](https://github.com/mapequation/infomap/commit/5dabf4abbaa90512d5a1ffd11c8986f8623ccb13))
* **python:** Add 'add_networkx_graph' method to API ([4067d40](https://github.com/mapequation/infomap/commit/4067d40268c4b2726652b5ef47358ca049c58edd))
* **python:** Add effective number of modules ([0f9a516](https://github.com/mapequation/infomap/commit/0f9a5169cacde5c9106bb6cf7dfa3b726a1c28d5))
* **python:** Add shorthand for accessing number of nodes and links ([8808045](https://github.com/mapequation/infomap/commit/88080454e054c51516ee7047975baf480ce1bd42))
* Add 'flow' to output options for node/edge flow ([d74a773](https://github.com/mapequation/infomap/commit/d74a773dd33102b85ff444158c958a8048c8cb02))


### Bug Fixes

* **python:** Also add nodes in networkx example ([3a88878](https://github.com/mapequation/infomap/commit/3a888789cc229521cffdf6da62fd0c239a5607eb))
* **python:** Fix effective_num_modules ([f41827f](https://github.com/mapequation/infomap/commit/f41827fde6f30e914e73d8a141992c4363f14adc))
* **R:** Expose base class methods to avoid c stack usage error ([47bdec0](https://github.com/mapequation/infomap/commit/47bdec0ab9a3d0e80cd7a77a7629ad9d9b400183))
* **R:** Fix swig build by hiding python specifics ([6b8a67e](https://github.com/mapequation/infomap/commit/6b8a67e6f09a02daad8e85c8b9fcec0f632526e3))
* **R:** Replace enum class with struct for R ([73c702f](https://github.com/mapequation/infomap/commit/73c702fd23e7a02ae8ccc3630f5713181cb842a9))
* **R:** Update R example and build to Infomap v1 ([17e1cc0](https://github.com/mapequation/infomap/commit/17e1cc00bd789c8798bd0633a39285a4160854a4))
* **R:** Update swig version in docker build ([74c4e86](https://github.com/mapequation/infomap/commit/74c4e86cfdd395ec1b335fc34dd5e0b34d7f6c1e))

## [1.3.0](https://github.com/mapequation/infomap/compare/v1.2.1...v1.3.0) (2020-12-03)


### Features

* Add --hide-bipartite-nodes ([#156](https://github.com/mapequation/infomap/issues/156)) ([f5e673c](https://github.com/mapequation/infomap/commit/f5e673c64efaf5ae825ee6b303e78389b8d594f9))
* New optional flow model for bipartite input ([91f35fe](https://github.com/mapequation/infomap/commit/91f35fef9b1246b87cde46d8e36ad7a992291a28))
* Support Newick tree format in output ([9749a05](https://github.com/mapequation/infomap/commit/9749a0535b9db47826c198b6f98b8fe2510eadc4))


### Bug Fixes

* Continue searching from input tree ([2199921](https://github.com/mapequation/infomap/commit/21999211ed822a8e46a8f1cd158185ce79b3075e))
* Fine-tune bottom modules in input tree with -F ([c187718](https://github.com/mapequation/infomap/commit/c187718e49618e2dac6c060233c844ff9a40893f))
* Implement missing multilayer relax by JSD ([95141bf](https://github.com/mapequation/infomap/commit/95141bf89452053a5f8f7bb2afbfc4048f066ee3)), closes [#154](https://github.com/mapequation/infomap/issues/154)
* Use correct objective in coarse tune for memory and multilevel networks ([97db558](https://github.com/mapequation/infomap/commit/97db558f27a81994a09aa6834bff5d8810a93130)), closes [#174](https://github.com/mapequation/infomap/issues/174)

### [1.2.1](https://github.com/mapequation/infomap/compare/v1.2.0...v1.2.1) (2020-11-10)


### Bug Fixes

* --clu-level was not respected in command line usage ([b82a0b1](https://github.com/mapequation/infomap/commit/b82a0b1a0582583b4552887650b0a6b92aba14fc)), closes [#153](https://github.com/mapequation/infomap/issues/153)
* Allow ftree cluster data ([bfe3601](https://github.com/mapequation/infomap/commit/bfe3601a50675cf353379efdb940b184773fb2b2))
* Apply weight-threshold also to multilayer nodes ([db2d781](https://github.com/mapequation/infomap/commit/db2d781c5257932ad5a4e5fe1976498b01985a39))
* Remove ClusterReader/BipartiteClusterReader ([8e08309](https://github.com/mapequation/infomap/commit/8e0830915f47bd4460a1b1b4f3794d60919a429a)), closes [#162](https://github.com/mapequation/infomap/issues/162)
* **windows:** Include algorithm header ([4f9ef88](https://github.com/mapequation/infomap/commit/4f9ef88409012ff00457291fc0ad8d0f33063c59)), closes [#150](https://github.com/mapequation/infomap/issues/150)

## [1.2.0](https://github.com/mapequation/infomap/compare/v1.1.4...v1.2.0) (2020-09-30)


### Features

* Add option to --use-node-weights-as-flow ([0d75174](https://github.com/mapequation/infomap/commit/0d751745ba565e06f93dfad6feb79ac9c6794e1e))
* Show bipartite start id in bipartite output ([f6e9233](https://github.com/mapequation/infomap/commit/f6e9233932f39f2dbe72b842fe2ddb259b725e08))


### Bug Fixes

* Fix flow calculator for bipartite networks ([b725b27](https://github.com/mapequation/infomap/commit/b725b27563e84092438839606aba294d7b993de5))
* **python:** Fix bipartite_start_id getter ([d326298](https://github.com/mapequation/infomap/commit/d326298c22570ded7002a8ee9cc361a448d8e4c5))

### [1.1.4](https://github.com/mapequation/infomap/compare/v1.1.3...v1.1.4) (2020-09-13)


### Bug Fixes

* Clarify that the algorithm searches for multi-level solutions by default ([47be4de](https://github.com/mapequation/infomap/commit/47be4defa349e3be99851105aff290cd488b9282))
* **python:** Fix color indices in networkx example ([dcf5888](https://github.com/mapequation/infomap/commit/dcf588800faf7f5d1f9be13d0bcc5528a2c2cf18))

### [1.1.3](https://github.com/mapequation/infomap/compare/v1.1.2...v1.1.3) (2020-05-09)


### Bug Fixes

* **python:** Correct codelength terms after N > 1 ([146cee6](https://github.com/mapequation/infomap/commit/146cee6246e55fb9c4a711c1278bdc251b54fced))
* **python:** Define module_codelength as L - L_index ([fa6eb94](https://github.com/mapequation/infomap/commit/fa6eb946acb22d808cd286bcfad6d76d3f2d4293))
* **python:** Don't create empty node names if they didn't already exist ([#126](https://github.com/mapequation/infomap/issues/126)) ([4d1cd56](https://github.com/mapequation/infomap/commit/4d1cd565e356f5c4ac4e8ce1620ae128a0f9ad4c))
* **python:** Expose num_non_trivial_top_modules ([6b91184](https://github.com/mapequation/infomap/commit/6b911842693f46546fdc89beb145394ed74dacdf))

### [1.1.2](https://github.com/mapequation/infomap/compare/v1.1.1...v1.1.2) (2020-04-08)


### Bug Fixes

* **python:** Expose node names with get_name and get_names ([#125](https://github.com/mapequation/infomap/issues/125)) ([b8cb409](https://github.com/mapequation/infomap/commit/b8cb409eec171d63f60634ce3a19cc01bc3a6eef)), closes [#52](https://github.com/mapequation/infomap/issues/52)

### [1.1.1](https://github.com/mapequation/infomap/compare/v1.1.0...v1.1.1) (2020-04-08)


### Bug Fixes

* Remove implied link direction from --skip-adjust-bipartite-flow ([05c3eba](https://github.com/mapequation/infomap/commit/05c3eba891521e6833292802c9c9671dee1303f7))

## [1.1.0](https://github.com/mapequation/infomap/compare/v1.0.11...v1.1.0) (2020-03-31)


### Features

* Show num levels and modules in file output ([0b70cd8](https://github.com/mapequation/infomap/commit/0b70cd82030693faaf24a318d45b7e929dc61c77))
* **python:** Expose codelengths for all trials ([eab7ae8](https://github.com/mapequation/infomap/commit/eab7ae861b2d93a9aea640994b53a80f4a52030c))


### Bug Fixes

* Don't use physical names in _states_as_physical.net ([5889fb1](https://github.com/mapequation/infomap/commit/5889fb17b13a38b1ce7a3b5ae714810bba1406c2))

### [1.0.11](https://github.com/mapequation/infomap/compare/v1.0.10...v1.0.11) (2020-03-31)


### Bug Fixes

* Handle zero weights in intra-layer links ([ddc5a3d](https://github.com/mapequation/infomap/commit/ddc5a3d827013cc006bdfa4b3518c32aef2ec722))

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
