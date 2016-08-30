#pragma once

typedef boost::geometry::model::point<double, 3, boost::geometry::cs::spherical_equatorial<boost::geometry::degree>> wgs84_point_t;
typedef boost::geometry::model::segment<wgs84_point_t> wgs84_segment_t;
typedef boost::geometry::model::linestring<wgs84_point_t> wgs84_linestring_t;
typedef boost::geometry::model::box<wgs84_point_t> wgs84_box_t;
typedef std::pair<std::uint64_t, std::uint64_t> nodepair_t;
typedef std::pair<wgs84_segment_t, nodepair_t> rtree_value_t;
typedef boost::geometry::index::rtree<rtree_value_t, boost::geometry::index::rstar<16>> line_rtree_t;

enum ValidDirections {
    Both,
    Forward,
    Reverse
};
