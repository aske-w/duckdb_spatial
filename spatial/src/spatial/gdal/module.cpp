#include "spatial/gdal/module.hpp"
#include "spatial/gdal/functions.hpp"
#include "spatial/gdal/file_handler.hpp"
#include "spatial/common.hpp"

#include "ogrsf_frmts.h"

#include <mutex>

namespace spatial {

namespace gdal {

void GdalModule::Register(DatabaseInstance &db) {

	// Load GDAL (once)
	static std::once_flag loaded;
	std::call_once(loaded, [&]() {
		// Register all embedded drivers (dont go looking for plugins)
		OGRRegisterAllInternal();

		// Set GDAL error handler

		CPLSetErrorHandler([](CPLErr e, int code, const char *msg) {
			// DuckDB doesnt do warnings, so we only throw on errors
			if (e != CE_Failure && e != CE_Fatal) {
				return;
			}
			switch (code) {
			case CPLE_NoWriteAccess:
				throw PermissionException("GDAL Error (%d): %s", code, msg);
			case CPLE_UserInterrupt:
				throw InterruptException();
			case CPLE_OutOfMemory:
				throw OutOfMemoryException("GDAL Error (%d): %s", code, msg);
			case CPLE_NotSupported:
				throw NotImplementedException("GDAL Error (%d): %s", code, msg);
			case CPLE_AssertionFailed:
			case CPLE_ObjectNull:
				throw InternalException("GDAL Error (%d): %s", code, msg);
			case CPLE_IllegalArg:
				throw InvalidInputException("GDAL Error (%d): %s", code, msg);
			case CPLE_AppDefined:
			case CPLE_HttpResponse:
			case CPLE_FileIO:
			case CPLE_OpenFailed:
			default:
				throw IOException("GDAL Error (%d): %s", code, msg);
			}
		});

		// Install the duckdb file handler
		GdalFileHandler::Register();
	});

	// Register functions
	GdalTableFunction::Register(db);
	GdalDriversTableFunction::Register(db);
	GdalCopyFunction::Register(db);
}

} // namespace gdal

} // namespace spatial
