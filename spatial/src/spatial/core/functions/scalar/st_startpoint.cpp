#include "spatial/common.hpp"
#include "spatial/core/types.hpp"
#include "spatial/core/functions/scalar.hpp"
#include "spatial/core/functions/common.hpp"
#include "spatial/core/geometry/geometry.hpp"

#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/common/vector_operations/unary_executor.hpp"
#include "duckdb/common/vector_operations/binary_executor.hpp"

namespace spatial {

namespace core {

//------------------------------------------------------------------------------
// LINESTRING_2D
//------------------------------------------------------------------------------
static void LineStringStartPointFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto geom_vec = args.data[0];
	auto count = args.size();

	UnifiedVectorFormat geom_format;
	geom_vec.ToUnifiedFormat(count, geom_format);

	auto line_vertex_entries = ListVector::GetData(geom_vec);
	auto &line_vertex_vec = ListVector::GetEntry(geom_vec);
	auto &line_vertex_vec_children = StructVector::GetEntries(line_vertex_vec);
	auto line_x_data = FlatVector::GetData<double>(*line_vertex_vec_children[0]);
	auto line_y_data = FlatVector::GetData<double>(*line_vertex_vec_children[1]);

	auto &point_vertex_children = StructVector::GetEntries(result);
	auto point_x_data = FlatVector::GetData<double>(*point_vertex_children[0]);
	auto point_y_data = FlatVector::GetData<double>(*point_vertex_children[1]);

	for (idx_t out_row_idx = 0; out_row_idx < count; out_row_idx++) {
		auto in_row_idx = geom_format.sel->get_index(out_row_idx);

		if (!geom_format.validity.RowIsValid(in_row_idx)) {
			FlatVector::SetNull(result, out_row_idx, true);
			continue;
		}

		auto line = line_vertex_entries[in_row_idx];
		auto line_offset = line.offset;
		auto line_length = line.length;

		if (line_length == 0) {
			FlatVector::SetNull(result, out_row_idx, true);
			continue;
		}

		point_x_data[out_row_idx] = line_x_data[line_offset];
		point_y_data[out_row_idx] = line_y_data[line_offset];
	}
	if (count == 1) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}
//------------------------------------------------------------------------------
// GEOMETRY
//------------------------------------------------------------------------------
static void GeometryStartPointFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &lstate = GeometryFunctionLocalState::ResetAndGet(state);
	auto &geom_vec = args.data[0];
	auto count = args.size();

	UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
	    geom_vec, result, count, [&](string_t input, ValidityMask &mask, idx_t row_idx) {
		    auto header = GeometryHeader::Get(input);
		    if (header.type != GeometryType::LINESTRING) {
			    mask.SetInvalid(row_idx);
			    return string_t();
		    }

		    auto line = lstate.factory.Deserialize(input).GetLineString();
		    auto point_count = line.Count();

		    if (point_count == 0) {
			    mask.SetInvalid(row_idx);
			    return string_t();
		    }

		    auto point = line.Vertices().Get(0);
		    return lstate.factory.Serialize(result, Geometry(lstate.factory.CreatePoint(point.x, point.y)));
	    });
}

//------------------------------------------------------------------------------
// Register functions
//------------------------------------------------------------------------------
void CoreScalarFunctions::RegisterStStartPoint(DatabaseInstance &db) {
	ScalarFunctionSet set("ST_StartPoint");

	set.AddFunction(ScalarFunction({GeoTypes::GEOMETRY()}, GeoTypes::GEOMETRY(), GeometryStartPointFunction, nullptr,
	                               nullptr, nullptr, GeometryFunctionLocalState::Init));

	set.AddFunction(ScalarFunction({GeoTypes::LINESTRING_2D()}, GeoTypes::POINT_2D(), LineStringStartPointFunction));

	ExtensionUtil::RegisterFunction(db, set);
}

} // namespace core

} // namespace spatial
