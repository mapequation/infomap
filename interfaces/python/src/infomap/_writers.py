import os


class _InfomapWritersMixin:
    def write(self, filename, *args, **kwargs):
        """Write results to file.

        Raises
        ------
        NotImplementedError
            If the file format is not supported.

        Parameters
        ----------
        filename : str
            The filename.
        """
        _, ext = os.path.splitext(filename)

        # remove the dot
        ext = ext[1:]

        if ext == "ftree":
            ext = "flow_tree"
        elif ext == "nwk":
            ext = "newick"
        elif ext == "net":
            ext = "pajek"

        writer = "write_{}".format(ext)

        if hasattr(self, writer):
            return getattr(self, writer)(filename, *args, **kwargs)

        raise NotImplementedError("No method found for writing {} files".format(ext))

    def write_clu(self, filename, states=False, depth_level=1):
        """Write result to a clu file.

        See Also
        --------
        write_tree
        write_flow_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        depth_level : int, optional
            The depth in the hierarchical tree to write.
        """
        return self.writeClu(filename, states, depth_level)

    def write_tree(self, filename, states=False):
        """Write result to a tree file.

        See Also
        --------
        write_clu
        write_flow_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        return self.writeTree(filename, states)

    def write_flow_tree(self, filename, states=False):
        """Write result to a ftree file.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        return self.writeFlowTree(filename, states)

    def write_newick(self, filename, states=False):
        """Write result to a Newick file.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        return self.writeNewickTree(filename, states)

    def write_json(self, filename, states=False):
        """Write result to a JSON file.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        return self.writeJsonTree(filename, states)

    def write_csv(self, filename, states=False):
        """Write result to a CSV file.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        return self.writeCsvTree(filename, states)

    def write_state_network(self, filename):
        """Write internal state network to file.

        See Also
        --------
        write_pajek

        Parameters
        ----------
        filename : str
        """
        return self.network.writeStateNetwork(filename)

    def write_pajek(self, filename, flow=False):
        """Write network to a Pajek file.

        See Also
        --------
        write_state_network

        Parameters
        ----------
        filename : str
        flow : bool, optional
            If the flow should be included. Default ``False``.
        """
        return self.network.writePajekNetwork(filename, flow)
