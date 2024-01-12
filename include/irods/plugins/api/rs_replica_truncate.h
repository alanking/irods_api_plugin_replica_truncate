#ifndef IRODS_RS_REPLICA_TRUNCATE_HPP
#define IRODS_RS_REPLICA_TRUNCATE_HPP

struct RsComm;

int rs_replica_truncate(RsComm* _comm, const char* _input, char** _output);

#endif // IRODS_RS_REPLICA_TRUNCATE_HPP
