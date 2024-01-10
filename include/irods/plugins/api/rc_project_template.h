#ifndef IRODS_RC_PROJECT_TEMPLATE_H
#define IRODS_RC_PROJECT_TEMPLATE_H

struct RcComm;

extern "C" int rc_project_template(struct RcComm* _comm, const char* _message, char** _response);

#endif // IRODS_RC_PROJECT_TEMPLATE_H
