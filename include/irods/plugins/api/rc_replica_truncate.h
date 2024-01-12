#ifndef IRODS_RC_REPLICA_TRUNCATE_H
#define IRODS_RC_REPLICA_TRUNCATE_H

struct RcComm;

extern "C" int rc_replica_truncate(struct RcComm* _comm, const char* _input, char** _output);

#endif // IRODS_RC_REPLICA_TRUNCATE_H
