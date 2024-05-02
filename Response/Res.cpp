#include "Res.hpp"

Response::Response(){
	_statusCode = "200";
	_message = "OK";
	_contentType = "";
	_contentLength = "";
	_responseHead = "";
	_response = "";
	_errorfileGood = 0;
	_file.open("", std::ios::in);
	_oldURI = "";
	_URI = "";
	_extensions.clear();
	_isDirectory = false;
	_isHeader = false;
	_fileSize = 0;
	convertExtention();
}
Response::~Response(){
	_file.close();
}

void Response::sendResponse(std::map<int, Webserve>&multi_fd ,int fd){
	char *buff = new char[BUFFER_SIZE];
	if (_statusCode == "301") {
		std::string relativeURI = _URI.substr(_URI.find("/", 22));
		_responseHead += "HTTP/1.1 " + _statusCode + " " + _message + "\r\n";
		_responseHead += "Location: " + relativeURI + "\r\n";
		_responseHead += "Content-Length: " + to_string(0) + "\r\n\r\n";
		_isHeader = true;
		send(fd, _responseHead.c_str(), _responseHead.length(), 0);
		close(fd);
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
		multi_fd.erase(fd);
		return;
	}
	if (_statusCode == "404" || _statusCode == "403" || _statusCode == "408" || _statusCode == "500"
			|| _statusCode == "501" || _statusCode == "400" || _statusCode == "413"
			|| (_statusCode == "200" && multi_fd[fd].response_success == true)){
		createHtmlResponse(multi_fd ,fd, buff);//** send simple html page
		return ;
		
	}
	if(!_isHeader && !_isDirectory){
		_file.open(_URI.c_str(), std::ios::in | std::ios::out);
		if (!_file.good() && !_isDirectory) { // file not found
			_errorfileGood = errno;
			_statusCode = "403";
			_message = "Forbidden";
			if (!_isHeader){
				createHeader("0");
				if (send(fd, _responseHead.c_str(), _responseHead.length(), 0) == -1){
					close(fd);
					epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
					multi_fd.erase(fd);
					return ;
				}
			}
			close(fd);
			return ;
		}
		// file found
		if (_statusCode == "200" && !_isDirectory){
			_file.seekg(0, std::ios::end);
			std::streampos fileSize = _file.tellg();
			_file.seekg(0, std::ios::beg);
			_fileSize = static_cast<size_t>(fileSize);
			createHeaderChunk();// header dyl file
		}
		if (send(fd, _responseHead.c_str(), _responseHead.length(), 0) == -1){
			close(fd);
			epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
			multi_fd.erase(fd);
			return ;
		}
	}
	else if (_statusCode == "200" && _isDirectory){//listing directory content
		findFiles(fd);
		close (fd);
		epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
		multi_fd.erase(fd);
		return ;
	}
	else if ((_statusCode == "200") && !_isDirectory) {//chunked start
		char buff[BUFFER_SIZE];
		if (!checkHeader(_responseHead)){
			_statusCode == "400";
			_message = "Bad Request";
			createHtmlResponse(multi_fd ,fd, buff);
			return ;
		}
		_file.read(buff, BUFFER_SIZE);
		std::streamsize bytesRead = _file.gcount();
		if (bytesRead > 0) {
			std::ostringstream chunkHeader;
			chunkHeader << std::hex << bytesRead << "\r\n";
			std::string chunkHeaderStr = chunkHeader.str();

			if (send(fd, chunkHeaderStr.c_str(), chunkHeaderStr.size(), 0) == -1) {
				close(fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
				multi_fd.erase(fd);
				return;
			}
			if (send(fd, buff, bytesRead, 0) == -1) {
				close(fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
				multi_fd.erase(fd);
				return;
			}
			const char* chunkEndMarker = "\r\n";
			if (send(fd, chunkEndMarker, 2, 0) == -1) {
				close(fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
				multi_fd.erase(fd);
				return;
			}

			if (bytesRead < BUFFER_SIZE)
				_file.seekg(-bytesRead, std::ios::cur);
		}
		else {
			const char* lastChunk = "0\r\n\r\n";
			if (send(fd, lastChunk, 5, 0) == -1) {
				close(fd);
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
				multi_fd.erase(fd);
				return;
			}
		}
	}
	return ;
}