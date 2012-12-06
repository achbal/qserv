# 
# LSST Data Management System
# Copyright 2012 LSST Corporation.
# 
# This product includes software developed by the
# LSST Project (http://www.lsst.org/).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the LSST License Statement and 
# the GNU General Public License along with this program.  If not, 
# see <http://www.lsstcorp.org/LegalNotices/>.
#
# lsst.qserv.master.spatial 
# This module contains the spatial indexing/partitioning logic needed
# at the Python master/frontend level. This contains only the parts
# needed to prepare the lower C++ code to rewrite the query properly.
import lsst.qserv.master.config
from lsst.qserv.master.geometry import SphericalBoxPartitionMap

def makePmap():
    c = lsst.qserv.master.config.config
    stripes = c.getint("partitioner", "stripes")
    substripes = c.getint("partitioner", "substripes")
    if (stripes < 1) or (substripes < 1):
        msg = "Partitioner's stripes and substripes must be natural numbers."
        raise lsst.qserv.master.config.ConfigError(msg)
    p = SphericalBoxPartitionMap(stripes, substripes) 
    print "Using %d stripes and %d substripes." % (stripes, substripes)
    return p


########################################################################

class PartitioningConfig: 
    """ An object that stores information about the partitioning setup.
    """
    def __init__(self):
        self.clear() # reset fields

    def clear(self):
        ## public
        self.chunked = set([])
        self.subchunked = set([])
        self.allowedDbs = set([])
        self.chunkMapping = ChunkMapping()
        self.chunkMeta = ChunkMeta()
        pass

    def applyConfig(self):
        c = lsst.qserv.master.config.config
        try:
            chk = c.get("table", "chunked")
            subchk = c.get("table", "subchunked")
            adb = c.get("table", "alloweddbs")
            self.chunked.update(chk.split(","))
            self.subchunked.update(subchk.split(","))    
            self.allowedDbs.update(adb.split(","))
        except:
            print "Error: Bad or missing chunked/subchunked spec."
        self._updateMap()
        self._updateMeta()
        pass

    def getMapRef(self, chunk, subchunk):
        """@return a map reference suitable for sql parsing and substitution.
        For convenience.
        """
        return self.chunkMapping.getMapReference(chunk, subchunk)

    def _updateMeta(self):
        for db in self.allowedDbs:
            map(lambda t: self.chunkMeta.add(db, t, 1), self.chunked)
            map(lambda t: self.chunkMeta.add(db, t, 2), self.subchunked)
        pass

    def _updateMap(self):
        map(self.chunkMapping.addChunkKey, self.chunked)
        map(self.chunkMapping.addSubChunkKey, self.subchunked)
        pass

########################################################################

class RegionFactory:
    def __init__(self):
        self._constraintNames = {
            "box" : self._handleBox,
            "circle" : self._handleCircle,
            "ellipse" : self._handleEllipse,
            "poly": self._handleConvexPolygon,
            "hull": self._handleConvexHull,
            # Handled elsewhere
            "db" : self._handleNop,
            "objectId" : self._handleNop
            }
        pass

    def _splitParams(self, name, tupleSize, param):
        hList = map(float,param.split(","))
        assert 0 == (len(hList) % tupleSize), "Wrong # for %s." % name
        # Split a long param list into tuples.
        return map(lambda x: hList[tupleSize*x : tupleSize*(x+1)],
                   range(len(hList)/tupleSize))
        
    def _handleBox(self, param):
        # ramin, declmin, ramax, declmax
        return map(lambda t: SphericalBox(t[:2], t[2:]),
                   self._splitParams("box", 4, param))

    def _handleCircle(self, param):
        # ra,decl, radius
        return map(lambda t: SphericalBox(t[:2], t[2:]),
                   self._splitParams("circle", 3, param))

    def _handleEllipse(self, param):
        # ra,decl, majorlen,minorlen,majorangle
        return map(lambda t: SphericalBox(t[:2], t[2], t[3], t[4]),
                   self._splitParams("ellipse", 5, param))

    def _handleConvexPolygon(self, param):
        # For now, list of vertices only, in counter-clockwise order
        # vertex count, ra1,decl1, ra2,decl2, ra3,decl3, ... raN,declN
        # Note that:
        # N = vertex_count, and 
        # N >= 3 in order to be considered a polygon.
        return self._handlePointSequence(SphericalConvexPolygon,
                                         "convex polygon", param)

    def _handleConvexHull(self, param):
        # ConvexHull is adds a processing step to create a polygon from
        # an unordered set of points.
        # Points are given as ra,dec pairs:
        # point count, ra1,decl1, ra2,decl2, ra3,decl3, ... raN,declN
        # Note that:
        # N = point_count, and 
        # N >= 3 in order to define a hull with nonzero area.
        return self._handlePointSequence(convexHull, "convex hull", param)

    def _handlePointSequence(self, constructor, name, param):
        h = param.split(",")
        polys = []
        while true:
            count = int(h[0]) # counts are integer
            next = 1 + (2*count)
            assert len(hList) >= next, \
                "Not enough values for %s (%d)" % (name, count)
            flatPoints = map(float, h[1 : next])
            # A list of 2-tuples should be okay as a list of vertices.
            polys.append(constructor(zip(flatPoints[::2],
                                        flatPoints[1::2])))
            h = h[next:]
        return polys
        
    def _handleNop(self, param):
        return []

    def getRegionFromHint(self, hintDict):
        """
        Convert a hint string list into a list of geometry regions that
        can be used with a partition mapper.
        
        @param hintDict a dictionary of hints
        @return None on error
        @return list of spherical regions if successful
        """
        regions = []
        try:
            for name,param in hintDict.items():
                if name in self._constraintNames: # spec?
                    regs = self._constraintNames[name](param)
                    regions.extend(regs)
                else: # No previous type, and no current match--> error
                    self.errorDesc = "Bad Hint name found:"+name
                    return None
        except Exception, e:
            self.errorDesc = e.message
            return None

        return regions
                                          
#########################################################################
# Shared:
########################################################################
_spatialConfig = None
_rFactory = None
########################################################################
## External interface:
########################################################################


def getSpatialConfig():
    if not _spatialConfig:
        _spatialConfig = PartitioningConfig()
        _spatialConfig.applyConfig()
    return _spatialConfig

def getRegionFactory():
    if not _rFactory:
        _rFactory = RegionFactory()
    return _rFactory()
