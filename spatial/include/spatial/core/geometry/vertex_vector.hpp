#pragma once

#include "spatial/common.hpp"
#include "spatial/core/types.hpp"
#include <cmath>

namespace spatial {

namespace core {

class Cursor;
struct BoundingBox;

struct Vertex {
	double x;
	double y;
	explicit Vertex() : x(0), y(0) {
	}
	explicit Vertex(double x, double y) : x(x), y(y) {
	}

	// Distance to another point
	double Distance(const Vertex &other) const;

	// Squared distance to another point
	double DistanceSquared(const Vertex &other) const;

	// Distance to the line segment between p1 and p2
	double Distance(const Vertex &p1, const Vertex &p2) const;

	// Squared distance to the line segment between p1 and p2
	double DistanceSquared(const Vertex &p1, const Vertex &p2) const;

	bool operator==(const Vertex &other) const {
		// approximate comparison
		return std::fabs(x - other.x) < 1e-10 && std::fabs(y - other.y) < 1e-10;
	}

	bool operator!=(const Vertex &other) const {
		return x != other.x || y != other.y;
	}

	Side SideOfLine(const Vertex &p1, const Vertex &p2) const {
		double side = ((x - p1.x) * (p2.y - p1.y) - (p2.x - p1.x) * (y - p1.y));
		if (side == 0) {
			return Side::ON;
		} else if (side < 0) {
			return Side::LEFT;
		} else {
			return Side::RIGHT;
		}
	}

	// Returns true if the point is on the line between the two points
	bool IsOnSegment(const Vertex &p1, const Vertex &p2) const {
		return ((p1.x <= x && x < p2.x) || (p1.x >= x && x > p2.x)) ||
		       ((p1.y <= y && y < p2.y) || (p1.y >= y && y > p2.y));
	}
};

enum class WindingOrder { CLOCKWISE, COUNTER_CLOCKWISE };

enum class Contains { INSIDE, OUTSIDE, ON_EDGE };

class VertexVector {
public:
	uint32_t count;
	uint32_t capacity;
	data_ptr_t data;

	explicit VertexVector(data_ptr_t data, uint32_t count, uint32_t capacity)
	    : count(count), capacity(capacity), data(data) {
	}

	// Create a VertexVector from an already existing buffer
	static VertexVector FromBuffer(data_ptr_t buffer, uint32_t count) {
		VertexVector array(buffer, count, count);
		return array;
	}

	inline uint32_t Count() const {
		return count;
	}

	inline uint32_t Capacity() const {
		return capacity;
	}

	inline void Add(const Vertex &v) {
		D_ASSERT(count < capacity);
		Store<Vertex>(v, data + count * sizeof(Vertex));
		count++;
	}

	inline void Set(uint32_t index, const Vertex &v) const {
		D_ASSERT(index < count);
		Store<Vertex>(v, data + index * sizeof(Vertex));
	}

	inline Vertex Get(uint32_t index) const {
		D_ASSERT(index < count);
		return Load<Vertex>(data + index * sizeof(Vertex));
	}

	// Returns the number of bytes that this VertexVector requires to be serialized
	inline uint32_t SerializedSize() const {
		return sizeof(Vertex) * count;
	}

	void Serialize(Cursor &cursor) const;
	void SerializeAndUpdateBounds(Cursor &cursor, BoundingBox &bbox) const;

	double Length() const;
	double SignedArea() const;
	double Area() const;
	Contains ContainsVertex(const Vertex &p, bool ensure_closed = true) const;
	WindingOrder GetWindingOrder() const;
	bool IsClockwise() const;
	bool IsCounterClockwise() const;

	// Returns true if the VertexVector is closed (first and last point are the same)
	bool IsClosed() const;

	// Returns true if the VertexVector is empty
	bool IsEmpty() const;

	// Returns true if the VertexVector is simple (no self-intersections)
	bool IsSimple() const;

	// Returns the index and distance of the closest segment to the point
	std::tuple<uint32_t, double> ClosestSegment(const Vertex &p) const;

	// Returns the index and distance of the closest point in the pointarray to the point
	std::tuple<uint32_t, double> ClosetVertex(const Vertex &p) const;

	// Returns the closest point, how far along the pointarray it is (0-1), and the distance to that point
	std::tuple<Vertex, double, double> LocateVertex(const Vertex &p) const;
};

// Utils
Vertex ClosestPointOnSegment(const Vertex &p, const Vertex &p1, const Vertex &p2);

} // namespace core

} // namespace spatial