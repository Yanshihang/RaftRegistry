//
// File created on: 2024/03/21
// Author: Zizhou

#ifndef RR_FILE_SERVLET_H
#define RR_FILE_SERVLET_H

#include "RaftRegistry/http/servlet.h"

namespace RR::http {
class FileServlet : public Servlet {
public:
    using ptr = std::shared_ptr<FileServlet>;
    FileServlet(const std::string& path);

    int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;


private:
    std::string m_path;
};
}

#endif // RR_FILE_SERVLET_H