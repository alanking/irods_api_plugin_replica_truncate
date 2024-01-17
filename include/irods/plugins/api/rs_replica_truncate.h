#ifndef IRODS_RS_REPLICA_TRUNCATE_HPP
#define IRODS_RS_REPLICA_TRUNCATE_HPP

struct RsComm;

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
/// \param[in] _comm iRODS server connection object
/// \param[in] _input \parblock JSON structure describing inputs to the operation. Should take the following form:
/// 	\code{.js}
/// 	{
/// 	    "path": "<string>",
/// 	    "length": <integer>,
/// 	    "target_resource": "<string>",
/// 	    "replica_number": <integer>
/// 	}
/// 	\endcode
///
/// 	 "path" - The logical path of the replica to truncate.
/// 	 "length" - The length to which the replica should be truncated. Behaves like truncate(2).
/// 		 If the file previously was larger than this size, the extra data is lost.
/// 		 If the file previously was shorter, it is extended, and the extended part
/// 		 reads as null bytes ('\0'). The value must be in the range [0,2147483648).
/// 	 "target_resource" - The root of the resource hierarchy hosting the target replica. A
/// 		 resource hierarchy resolution occurs using a "write" operation. This input is optional.
/// 		 An error will occur if this option is used with the replica_number option.
/// 	 "replica_number" - The replica number of the replica which is being truncated. The replica
/// 		 number must be in the range [0,2147483648). This input is optional. An error will occur
/// 		 if this option is used with the target_resource option.
/// \endparblock
/// \param[out] _output \parblock JSON structure describing outputs from the operation. Should take the following form:
/// 	\code{.js}
/// 	{
/// 	    "error_code": <integer>,
/// 	    "message": "<string>"
/// 	}
/// 	\endcode
///
/// 	"error_code" - The iRODS error code returned from the operation. If set, the errno will be in the last 3 digits.
/// 	"message" - A descriptive error or informational message from the operation. Usually empty on success.
/// \endparblock
///
/// \retval 0 on success
/// \retval -1 on failure
int rs_replica_truncate(RsComm* _comm, const char* _input, char** _output);

#endif // IRODS_RS_REPLICA_TRUNCATE_HPP
