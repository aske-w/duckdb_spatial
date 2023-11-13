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

static void DumpFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &lstate = GeometryFunctionLocalState::ResetAndGet(state);
	auto count = args.size();

	auto &geom_vec = args.data[0];
	UnifiedVectorFormat geom_format;
	geom_vec.ToUnifiedFormat(count, geom_format);

	idx_t total_geom_count = 0;
	idx_t total_path_count = 0;

	for (idx_t out_row_idx = 0; out_row_idx < count; out_row_idx++) {
		auto in_row_idx = geom_format.sel->get_index(out_row_idx);

		if (!geom_format.validity.RowIsValid(in_row_idx)) {
			FlatVector::SetNull(result, out_row_idx, true);
			continue;
		}

		auto geometry_blob = UnifiedVectorFormat::GetData<string_t>(geom_format)[in_row_idx];
		auto geometry = lstate.factory.Deserialize(geometry_blob);

		vector<std::tuple<Geometry, vector<int32_t>>> stack;
		vector<std::tuple<Geometry, vector<int32_t>>> items;

		stack.emplace_back(geometry, vector<int32_t>());

		while (!stack.empty()) {
			auto current = stack.back();
			auto current_geom = std::get<0>(current);
			auto current_path = std::get<1>(current);

			stack.pop_back();

			if (current_geom.Type() == GeometryType::MULTIPOINT) {
				auto mpoint = current_geom.GetMultiPoint();
				for (int32_t i = 0; i < mpoint.Count(); i++) {
					auto path = current_path;
					path.push_back(i + 1); // path is 1-indexed
					stack.emplace_back(mpoint[i], path);
				}
			} else if (current_geom.Type() == GeometryType::MULTILINESTRING) {
				auto mline = current_geom.GetMultiLineString();
				for (int32_t i = 0; i < mline.Count(); i++) {
					auto path = current_path;
					path.push_back(i + 1);
					stack.emplace_back(mline[i], path);
				}
			} else if (current_geom.Type() == GeometryType::MULTIPOLYGON) {
				auto mpoly = current_geom.GetMultiPolygon();
				for (int32_t i = 0; i < mpoly.Count(); i++) {
					auto path = current_path;
					path.push_back(i + 1);
					stack.emplace_back(mpoly[i], path);
				}
			} else if (current_geom.Type() == GeometryType::GEOMETRYCOLLECTION) {
				auto collection = current_geom.GetGeometryCollection();
				for (int32_t i = 0; i < collection.Count(); i++) {
					auto path = current_path;
					path.push_back(i + 1);
					stack.emplace_back(collection[i], path);
				}
			} else {
				items.push_back(current);
			}
		}

		// Finally reverse the results
		std::reverse(items.begin(), items.end());

		// Push to the result vector
		auto result_entries = ListVector::GetData(result);

		auto geom_offset = total_geom_count;
		auto geom_length = items.size();

		result_entries[out_row_idx].length = geom_length;
		result_entries[out_row_idx].offset = geom_offset;

		total_geom_count += geom_length;

		ListVector::Reserve(result, total_geom_count);
		ListVector::SetListSize(result, total_geom_count);

		auto &result_list = ListVector::GetEntry(result);
		auto &result_list_children = StructVector::GetEntries(result_list);
		auto &result_geom_vec = result_list_children[0];
		auto &result_path_vec = result_list_children[1];

		auto geom_data = FlatVector::GetData<string_t>(*result_geom_vec);
		for (idx_t i = 0; i < geom_length; i++) {
			// Write the geometry
			auto &item_blob = std::get<0>(items[i]);
			geom_data[geom_offset + i] = lstate.factory.Serialize(*result_geom_vec, item_blob);

			// Now write the paths
			auto &path = std::get<1>(items[i]);
			auto path_offset = total_path_count;
			auto path_length = path.size();

			total_path_count += path_length;

			ListVector::Reserve(*result_path_vec, total_path_count);
			ListVector::SetListSize(*result_path_vec, total_path_count);

			auto path_entries = ListVector::GetData(*result_path_vec);

			path_entries[geom_offset + i].offset = path_offset;
			path_entries[geom_offset + i].length = path_length;

			auto &path_data_vec = ListVector::GetEntry(*result_path_vec);
			auto path_data = FlatVector::GetData<int32_t>(path_data_vec);

			for (idx_t j = 0; j < path_length; j++) {
				path_data[path_offset + j] = path[j];
			}
		}
	}

	if (count == 1) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

void CoreScalarFunctions::RegisterStDump(DatabaseInstance &db) {
	ScalarFunctionSet set("ST_Dump");

	set.AddFunction(
	    ScalarFunction({GeoTypes::GEOMETRY()},
	                   LogicalType::LIST(LogicalType::STRUCT(
	                       {{"geom", GeoTypes::GEOMETRY()}, {"path", LogicalType::LIST(LogicalType::INTEGER)}})),
	                   DumpFunction, nullptr, nullptr, nullptr, GeometryFunctionLocalState::Init));

	ExtensionUtil::RegisterFunction(db, set);
}

} // namespace core

} // namespace spatial
