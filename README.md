  _This project has been created as part of the 42 curriculum by helin and yanzhao._

# Webserv 

## Description
Webserv is an HTTP/1.1 web server built in C++98. It can host static websites and run CGI scripts just like Nginx.

### Project Goal 
---
To understand how network protocols work and to implement a fully functional HTTP server from scratch.

### Key Features
---
* **I/O Multiplexing**: Non-blocking connection management using `poll()`.
* **Nginx-Style Config**: Easy server setup via a simple configuration file.
* **Supported Methods**: Full support for `GET`, `POST`, `HEAD`, and `DELETE`.
* **Virtual Hosting**: Hosts multiple websites on the same port via the `Host` header.
* **Static File Serving**: Fast delivery of HTML, CSS, JS.
* **Autoindexing**: Automatically lists directory contents if `index.html` is missing.
* **File Management**: Supports file uploads (`POST`) and file removal (`DELETE`).
* **CGI Gateway**: Executes Python and PHP scripts for dynamic content.
* **Redirect Handling**: Supports URL forwarding with HTTP `301`/`302`/`303`/`307`/`308` status codes.
* **Session & Cookies**: Handles user login, authentication, and logout via Python CGI.



## Instructions
### Prerequistes
---
You need a C++ compiler (`c++`) and `make`.

### Compilation & Management
You can manage the project compilation using the following `Makefile` commands:

```bash
# Compile the webserv executable (C++98 compliant)
make

# Remove all object files (.o)
make clean

# Remove all object files and the webserv executable
make fclean

# Recompile the entire project from scratch
make re
```

### Runing the Server

```./webserv ./conf/default.conf```

### Quick test

```curl -v http://localhost:8080/``` (GET)

```curl -I http://localhost:8080/``` (HEAD)

```curl -v -X POST -d "this is a test.txt file" POST http://localhost:8080/uploads/test.txt``` (POST)

```curl -X POST http://localhost:8080/uploads -F "file=@/home/login_name/images.jpg" -v``` (POST jpg)

```curl -v -X DELETE http://localhost:8080/uploads/test.txt``` (DELETE)


### Siege test
TODO

### Tstest run
TODO

### Pytest run
TODO

# Resources
TODO
* [xxx] line hypertext - purpose 


 `how do we use AI to build up the project`
 TODO