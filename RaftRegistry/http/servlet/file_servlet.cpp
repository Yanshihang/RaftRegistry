//
// File created on: 2024/03/21
// Author: Zizhou

#include "RaftRegistry/http/servlet/file_servlet.h"

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace RR::http {
FileServlet::FileServlet(const std::string& path) : Servlet("FileServlet"), m_path(path) {}

int32_t FileServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
    // 检查请求的路径是否包含 ".."，防止目录遍历攻击
    if (request->getPath().find("..") != std::string::npos) {
        NotFoundServlet("RaftRegistry/1.0.0").handle(request, response, session);
        return 1; // 处理完成
    }

    // 拼接完整的文件路径
    // 所有使用fileservlet功能进行文件传输的文件都必须存储在当前fileservlet实例中指定的目录下
    std::string fileName = m_path + request->getPath();
    
    struct stat filestat;
    // 获取文件状态，如果失败则使用 NotFoundServlet 处理请求
    if (stat(fileName.c_str(), &filestat) < 0) {
        NotFoundServlet("RaftRegistry/1.0.0").handle(request, response, session);
        return 1;
    }

    // 检查文件是否是普通文件且可读，否则使用 NotFoundServlet 处理
    if (!S_ISREG(filestat.st_mode) || !(S_IRUSR & filestat.st_mode)) {
        NotFoundServlet("RaftRegistry/1.0.0").handle(request, response, session);
        return 1;
    }

    // 这里实现的是将response分批写入socket。
    // 先调用sendresponse写入相应行和响应头，然后使用sendfile写入响应体
    response->setStatus(HttpStatus::OK);
    response->setHeader("Server", "RaftRegistry/1.0.0");
    response->setHeader("Content-length", std::to_string(filestat.st_size));
    session->sendResponse(response);

    // 打开文件，并使用 sendfile 函数发送文件内容
    int fileFd = open(fileName.c_str(), O_RDONLY);
    sendfile(session->getSocket()->getSocket(), fileFd, nullptr, filestat.st_size);

    return 1;
}

}