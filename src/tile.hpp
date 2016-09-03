#pragma once

#include <protozero/pbf_writer.hpp>
#include <protozero/varint.hpp>

#include <boost/geometry.hpp>

#include "web_mercator.hpp"
#include "vector_tile.hpp"
#include "common.hpp"

namespace util { namespace tile {

typedef boost::geometry::model::point<std::int32_t, 2, boost::geometry::cs::cartesian> tile_point_t;
typedef boost::geometry::model::linestring<tile_point_t> tile_linestring_t;
typedef boost::geometry::model::box<tile_point_t> tile_box_t;

typedef boost::geometry::model::point<double, 2, boost::geometry::cs::cartesian> mercator_point_t;
typedef boost::geometry::model::linestring<mercator_point_t> mercator_linestring_t;
typedef boost::geometry::model::box<mercator_point_t> mercator_box_t;
typedef boost::geometry::model::multi_linestring<mercator_linestring_t> mercator_multi_linestring_t;

const static tile_box_t tile_clip_box(tile_point_t(-util::vector_tile::BUFFER, -util::vector_tile::BUFFER),
                                      tile_point_t(util::vector_tile::EXTENT + util::vector_tile::BUFFER,
                                                   util::vector_tile::EXTENT + util::vector_tile::BUFFER));

struct tile_point_hash {
    std::size_t operator()(const tile_point_t &key) const {
        const auto a = std::hash<std::int32_t>()(key.get<0>());
        const auto b = std::hash<std::int32_t>()(key.get<1>());
        // From boost::hash_combine
        return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
    }
};

struct tile_point_equal {
    bool operator()(const tile_point_t &a, const tile_point_t &b) const {
        return a.get<0>() == b.get<0>() && a.get<1>() == b.get<1>();
    }
};



// from mapnik-vector-tile
// Encodes a linestring using protobuf zigzag encoding
inline bool encodeLinestring(const tile_linestring_t  &line,
                             protozero::packed_field_uint32 &geometry,
                             std::int32_t &start_x,
                             std::int32_t &start_y)
{
    const std::size_t line_size = line.size();
    if (line_size < 2)
    {
        return false;
    }

    const unsigned LINETO_count = static_cast<const unsigned>(line_size) - 1;

    auto pt = line.begin();
    const constexpr int MOVETO_COMMAND = 9;
    geometry.add_element(MOVETO_COMMAND); // move_to | (1 << 3)
    geometry.add_element(protozero::encode_zigzag32(pt->get<0>() - start_x));
    geometry.add_element(protozero::encode_zigzag32(pt->get<1>() - start_y));
    start_x = pt->get<0>();
    start_y = pt->get<1>();
    // This means LINETO repeated N times
    // See: https://github.com/mapbox/vector-tile-spec/tree/master/2.1#example-command-integers
    geometry.add_element((LINETO_count << 3u) | 2u);
    // Now that we've issued the LINETO REPEAT N command, we append
    // N coordinate pairs immediately after the command.
    for (++pt; pt != line.end(); ++pt)
    {
        const std::int32_t dx = pt->get<0>() - start_x;
        const std::int32_t dy = pt->get<1>() - start_y;
        geometry.add_element(protozero::encode_zigzag32(dx));
        geometry.add_element(protozero::encode_zigzag32(dy));
        start_x = pt->get<0>();
        start_y = pt->get<1>();
    }
    return true;
}

inline tile_linestring_t segmentToTileLine(const wgs84_segment_t &segment,
                                           const mercator_box_t &tile_bbox)
{
    wgs84_linestring_t geo_line;
    geo_line.push_back(segment.first);
    geo_line.push_back(segment.second);

    mercator_linestring_t unclipped_line;

    auto wgs84_to_tile = [&tile_bbox](const wgs84_point_t &wgs84_point) {
        // Convert lon/lat to global mercator coordinates
        double mercator_x = wgs84_point.get<0>() * util::web_mercator::DEGREE_TO_PX;
        double mercator_y = util::web_mercator::latToY(wgs84_point.get<1>()) *
                            util::web_mercator::DEGREE_TO_PX;

        // Convert global mercator coordinates to relative positions on
        // the provide mercator tile
        // convert lon/lat to tile coordinates
        const auto box_width = tile_bbox.max_corner().get<0>() - tile_bbox.min_corner().get<0>();
        const auto box_height = tile_bbox.max_corner().get<1>() - tile_bbox.min_corner().get<1>();
        const auto tile_x = std::round(
            ((mercator_x - tile_bbox.min_corner().get<0>()) * util::web_mercator::TILE_SIZE / box_width) *
            util::vector_tile::EXTENT / util::web_mercator::TILE_SIZE);
        const auto tile_y = std::round(
            ((tile_bbox.max_corner().get<1>() - mercator_y) * util::web_mercator::TILE_SIZE / box_height) *
            util::vector_tile::EXTENT / util::web_mercator::TILE_SIZE);

        return mercator_point_t(tile_x, tile_y);
    };

    boost::geometry::append(unclipped_line, wgs84_to_tile(segment.first));
    boost::geometry::append(unclipped_line, wgs84_to_tile(segment.second));

    mercator_multi_linestring_t clipped_line;

    boost::geometry::intersection(tile_clip_box, unclipped_line, clipped_line);

    tile_linestring_t tile_line;

    // b::g::intersection might return a line with one point if the
    // original line was very short and coords were dupes
    if (!clipped_line.empty() && clipped_line[0].size() == 2)
    {
        if (clipped_line[0].size() == 2)
        {
            for (const auto &p : clipped_line[0])
            {
                tile_line.emplace_back(p.get<0>(), p.get<1>());
            }
        }
    }

    return tile_line;
}

} }

