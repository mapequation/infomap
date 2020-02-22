from collections import namedtuple
from contextlib import contextmanager

MultilayerNode = namedtuple("MultilayerNode", "layer_id, node_id")


class Infomap(InfomapWrapper):
    def __init__(self, parameters=""):
        super().__init__(parameters)

    def read_file(self, filename, accumulate=True):
        super().readInputData(filename, accumulate)

    def add_node(self, node_id, name="", teleportation_weight=None):
        if (len(name) and teleportation_weight):
            super().addNode(node_id, name, teleportation_weight)
        elif (len(name) and not teleportation_weight):
            super().addNode(node_id, name)
        elif (teleportation_weight and not len(name)):
            super().addNode(node_id, teleportation_weight)
        else:
            super().addNode(node_id)

    def set_name(self, node_id, name):
        super().addName(node_id, name)

    def set_meta_data(self, node_id, meta_category):
        self.network.addMetaData(node_id, meta_category)

    def add_state_node(self, state_id, node_id):
        super().addStateNode(state_id, node_id)

    def add_link(self, source_id, target_id, weight=1.0):
        super().addLink(source_id, target_id, weight)

    def remove_link(self, source_id, target_id):
        self.network.removeLink(source_id, target_id)

    def add_multilayer_link(
            self,
            source_multilayer_node,
            target_multilayer_node,
            weight=1.0):
        source_layer_id, source_node_id = source_multilayer_node
        target_layer_id, target_node_id = target_multilayer_node
        super().addMultilayerLink(source_layer_id,
                                  source_node_id,
                                  target_layer_id,
                                  target_node_id,
                                  weight)

    @property
    def bipartite_start_id(self):
        return self.network.bipartiteStartId

    @bipartite_start_id.setter
    def bipartite_start_id(self, start_id):
        super().setBipartiteStartId(start_id)

    @property
    def initial_partition(self):
        return super().getInitialPartition()

    @initial_partition.setter
    def initial_partition(self, module_ids):
        if module_ids is None:
            module_ids = {}
        super().setInitialPartition(module_ids)

    @contextmanager
    def _initial_partition(self, partition):
        old_partition = self.initial_partition
        try:
            self.initial_partition = partition
            yield
        finally:
            self.initial_partition = old_partition

    def run(self, args="", initial_partition=None):
        if initial_partition:
            with self._initial_partition(initial_partition):
                super().run(args)
        else:
            super().run(args)

    def get_modules(self, depth_level=1, states=False):
        return super().getModules(depth_level, states)

    def get_multilevel_modules(self, states=False):
        return super().getMultilevelModules(states)

    @property
    def modules(self):
        return self.get_modules().items()

    def tree(self):
        return super().iterTree()

    @property
    def leaf_modules(self):
        return super().iterLeafModules()

    @property
    def leaf_nodes(self):
        return super().iterLeafNodes()

    @property
    def physical_tree(self):
        return super().iterTreePhysical()

    @property
    def physical_leaf_modules(self):
        return super().iterLeafModulesPhysical()

    @property
    def physical_leaf_nodes(self):
        return super().iterLeafNodesPhysical()

    @property
    def num_top_modules(self):
        return super().numTopModules()

    @property
    def max_depth(self):
        return super().maxTreeDepth()

    @property
    def codelength(self):
        """Total (hierarchical) codelength"""
        return super().codelength()

    @property
    def index_codelength(self):
        """Two-level index codelength"""
        return super().getIndexCodelength()

    @property
    def module_codelength(self):
        """Two-level module codelength"""
        return super().getModuleCodelength()

    @property
    def meta_codelength(self):
        """Meta codelength (meta entropy times meta data rate)"""
        return super().getMetaCodelength()

    @property
    def meta_entropy(self):
        """Meta entropy (unweighted by meta data rate)"""
        return super().getMetaCodelength(True)

    @property
    def network(self):
        return super().network()

    @property
    def nodes(self):
        """ Alias of leaf_nodes """
        return self.leaf_nodes

    def write_clu(self, filename="", states=False, depth_level=1):
        return self.writeClu(filename, states, depth_level)

    def write_tree(self, filename="", states=False):
        return self.writeTree(filename, states)

    def write_flow_tree(self, filename="", states=False):
        return self.writeFlowTree(filename, states)

    def set_no_infomap(self, value=True):
        return self.setNoInfomap(value)
