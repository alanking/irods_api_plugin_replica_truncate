#ifndef IRODS_RC_REPLICA_TRUNCATE_H
#define IRODS_RC_REPLICA_TRUNCATE_H

struct RcComm;
struct DataObjInp;
struct BytesBuf;

/// \brief Truncate a replica at the specified logical path to the specified length.
///
/// This operation may be performed on existing replicas on which the executing user has at least write permissions.
///
/// This API may cause the following resource plugin operations to execute:
///  resolve_resource_hierarchy
///  truncate
///
/// This API may cause the following database plugin operations to execute:
///  mod_data_obj_meta
///
/// \param[in] _comm iRODS client connection object
/// \param[in] _input \parblock Data object input structure. The following pieces should be included:
///		objPath - The logical path of the object.
///		dataSize - The length to which the replica should be truncated. Behaves like truncate(2).
/// 		 If the file previously was larger than this size, the extra data is lost.
/// 		 If the file previously was shorter, it is extended, and the extended part
/// 		 reads as null bytes ('\0'). The value must be in the range [0,2^63).
///		condInput - The following keywords will affect behavior:
/// 		- "rescName" - The root of the resource hierarchy hosting the target replica. A
/// 		 resource hierarchy resolution occurs using a "write" operation. This input is optional.
/// 		 An error will occur if this option is used with "replNum" option.
///			- "replNum" - The replica number of the replica which is being truncated. The replica
/// 		 number must be in the range [0,2^63). This input is optional. An error will occur
/// 		 if this option is used with the "rescName" option.
///			- "irodsAdmin" - If present, indicates that the user wishes to truncate the replica
///			 even if the user does not have permissions on the object. This input is optional. The
///			 default value is false. An error will occur if used by unprivileged uesrs.
/// \endparblock
/// \param[out] _output \parblock JSON structure describing outputs from the operation. Should take the following form:
/// 	\code{.js}
/// 	{
/// 	    "message": "<string>"
/// 	}
/// 	\endcode
///
/// 	"message" - A descriptive error or informational message from the operation. Usually empty on success.
/// \endparblock
///
/// \return iRODS error code.
/// \retval 0 on success
/// \retval <0 on failure
extern "C" int rc_replica_truncate(struct RcComm* _comm, const DataObjInp* _input, BytesBuf** _output);

#endif // IRODS_RC_REPLICA_TRUNCATE_H
