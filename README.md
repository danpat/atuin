# A'tuin
The great turtle that holds the world on its back.

## NOTE: below is the *plan*, not all of this is implemented yet

This is a vector tile server designed for high-performance real-time rendering of
vector tiles directly from an OSM file (XML or PBF formats).  It operates
entirely in RAM.

It has two purposes:

  - rendering plain vector tiles containing a configurable subset of the OSM
    features
  - rendering highly dynamic tiles.  Specifically designed for "last groomed" ski
    maps, but could also work well for "current traffic congestion" levels if you have
    the data for it.

Configuration files can select subsets of the network to load, and what features
are included at which zoom levels.  The aim is to have tiles rendered in <1ms
in most cases.  You will need lots of RAM.

## Dynamic Tile Rendering

When a tile is requested, the engine does the following:

  1. Determine the zoom level for the tile
  2. Select all segments that are on the tile and elibible for display at that zoom level.
  3. Using a configuration map, the freeflow and current speed for each segment are compared,
     and the differences are binned (e.g. "uncongested", "slightly slow", "very slow", "stopped").
  4. Connected segments with equal binning are joined into larger feature geometries.  This
     reduces the size of the eventual tile.
  5. Geometries are simplified according to the zoom level.  Overlapping single-pixel geometries
     are eliminated.
  5. The tile is returned to the user.

It is expected that a caching layer is put in front of this server.

## Dynamic data updates

`osm-tile-server` uses a large block of memory to hold the current speed values for all edges.
A separate tool `update-current-traffic` can be run with a CSV file and it will update the current
speed values in-place.  If tiles are requested while an update is occurring, the speed values
may be partly from the previous update and partly from the new update.

## Design notes

### Performance and in-memory data layouts

Segment data is sorted according to a space-filling-curve (e.g. Hilbert Curve).  Because we almost
exclusively select data using a tile, we can improve CPU cache hit rates by ensuring that likely
to be needed data is located in nearby memory when generating a tile.

### Tile geometry packing

- common property packing
- bidirectional attributes
- zoom level optimization
  The RTree uses a 3rd dimension to ensure fast queries.  When segments are added to the tree initially,
  the "maximum visible zoom level" is used as a 3rd axis.  This enables fast retrieval of relevant
  geometries.
- geometry simplification
  TODO: can we pre-determine that tiny geometries won't be visible for large zoom levels?
